#pragma once
#include <stdint.h>

struct ElfImage {
    uintptr_t entry_point;
    uintptr_t pml4_phys;
};

extern "C" {

// Loads an ELF64 executable from ramfs into a new user address space.
bool elf_load_from_ramfs(const char* name, ElfImage* image);

}
