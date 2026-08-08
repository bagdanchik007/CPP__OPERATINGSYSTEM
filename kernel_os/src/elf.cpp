#include "elf.h"
#include "pmm.h"
#include "ramfs.h"
#include "vmm.h"

namespace {
    constexpr uint8_t ELF_MAGIC_0 = 0x7F;
    constexpr uint8_t ELF_MAGIC_1 = 'E';
    constexpr uint8_t ELF_MAGIC_2 = 'L';
    constexpr uint8_t ELF_MAGIC_3 = 'F';
    constexpr uint8_t ELF_CLASS_64 = 2;
    constexpr uint8_t ELF_DATA_LITTLE_ENDIAN = 1;
    constexpr uint16_t ELF_MACHINE_X86_64 = 0x3E;
    constexpr uint32_t ELF_PROGRAM_HEADER_LOAD = 1;
    constexpr uint32_t ELF_FLAG_WRITABLE = 1 << 1;
    constexpr uintptr_t USER_ADDRESS_MIN = 0x40000000;

    struct __attribute__((packed)) Elf64Header {
        uint8_t ident[16];
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uint64_t entry;
        uint64_t program_header_offset;
        uint64_t section_header_offset;
        uint32_t flags;
        uint16_t header_size;
        uint16_t program_header_size;
        uint16_t program_header_count;
        uint16_t section_header_size;
        uint16_t section_header_count;
        uint16_t section_name_index;
    };

    struct __attribute__((packed)) Elf64ProgramHeader {
        uint32_t type;
        uint32_t flags;
        uint64_t offset;
        uint64_t virtual_address;
        uint64_t physical_address;
        uint64_t file_size;
        uint64_t memory_size;
        uint64_t alignment;
    };

    uintptr_t align_down(uintptr_t value) {
        return value & ~(PAGE_SIZE - 1);
    }

    uintptr_t align_up(uintptr_t value) {
        return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    void zero_memory(uint8_t* destination, size_t size) {
        for (size_t index = 0; index < size; index++) {
            destination[index] = 0;
        }
    }

    void copy_memory(uint8_t* destination, const uint8_t* source, size_t size) {
        for (size_t index = 0; index < size; index++) {
            destination[index] = source[index];
        }
    }

    uintptr_t current_address_space() {
        uintptr_t pml4_phys;
        asm volatile("mov %%cr3, %0" : "=r"(pml4_phys));
        return pml4_phys;
    }

    bool valid_user_range(uint64_t address, uint64_t size) {
        if (address < USER_ADDRESS_MIN || size > UINT64_MAX - address) {
            return false;
        }
        return address + size < 0x0000800000000000ULL;
    }
}

extern "C" bool elf_load_from_ramfs(const char* name, ElfImage* image) {
    if (!image) {
        return false;
    }

    const RamfsFile* file = ramfs_open(name);
    if (!file || file->size < sizeof(Elf64Header)) {
        return false;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(file->data);
    const Elf64Header* header = reinterpret_cast<const Elf64Header*>(data);
    if (header->ident[0] != ELF_MAGIC_0 || header->ident[1] != ELF_MAGIC_1 ||
        header->ident[2] != ELF_MAGIC_2 || header->ident[3] != ELF_MAGIC_3 ||
        header->ident[4] != ELF_CLASS_64 || header->ident[5] != ELF_DATA_LITTLE_ENDIAN ||
        header->machine != ELF_MACHINE_X86_64 ||
        header->program_header_size != sizeof(Elf64ProgramHeader) ||
        !valid_user_range(header->entry, 1)) {
        return false;
    }

    uint64_t program_headers_size =
        static_cast<uint64_t>(header->program_header_count) * sizeof(Elf64ProgramHeader);
    if (header->program_header_offset > file->size ||
        program_headers_size > file->size - header->program_header_offset) {
        return false;
    }

    uintptr_t pml4_phys = vmm_create_address_space();
    if (pml4_phys == 0) {
        return false;
    }

    page_table_t* user_pml4 = reinterpret_cast<page_table_t*>(pml4_phys);
    page_table_t* kernel_pml4 = reinterpret_cast<page_table_t*>(current_address_space());
    user_pml4->entries[0] = kernel_pml4->entries[0] | PTE_USER;

    const Elf64ProgramHeader* programs = reinterpret_cast<const Elf64ProgramHeader*>(
        data + header->program_header_offset);
    for (uint16_t index = 0; index < header->program_header_count; index++) {
        const Elf64ProgramHeader& program = programs[index];
        if (program.type != ELF_PROGRAM_HEADER_LOAD) {
            continue;
        }
        if (program.file_size > program.memory_size ||
            program.offset > file->size || program.file_size > file->size - program.offset ||
            !valid_user_range(program.virtual_address, program.memory_size)) {
            return false;
        }

        uint64_t flags = PTE_USER;
        if (program.flags & ELF_FLAG_WRITABLE) {
            flags |= PTE_WRITABLE;
        }

        uintptr_t segment_start = align_down(program.virtual_address);
        uintptr_t segment_end = align_up(program.virtual_address + program.memory_size);
        for (uintptr_t address = segment_start; address < segment_end; address += PAGE_SIZE) {
            uintptr_t page_phys = pmm_alloc_page();
            if (page_phys == 0) {
                return false;
            }
            zero_memory(reinterpret_cast<uint8_t*>(page_phys), PAGE_SIZE);
            vmm_map_page(pml4_phys, address, page_phys, flags);
        }

        uint64_t bytes_remaining = program.file_size;
        uintptr_t destination = program.virtual_address;
        const uint8_t* source = data + program.offset;
        while (bytes_remaining > 0) {
            uintptr_t destination_phys = vmm_translate(pml4_phys, destination);
            if (destination_phys == 0) {
                return false;
            }

            uint64_t page_remaining = PAGE_SIZE - (destination & (PAGE_SIZE - 1));
            uint64_t chunk_size = (bytes_remaining < page_remaining) ? bytes_remaining : page_remaining;
            copy_memory(reinterpret_cast<uint8_t*>(destination_phys), source,
                        static_cast<size_t>(chunk_size));
            destination += chunk_size;
            source += chunk_size;
            bytes_remaining -= chunk_size;
        }
    }

    image->entry_point = header->entry;
    image->pml4_phys = pml4_phys;
    return true;
}
