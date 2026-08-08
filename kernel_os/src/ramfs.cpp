#include "ramfs.h"

namespace {
    constexpr size_t MAX_RAMFS_FILES = 16;
    constexpr size_t MAX_RAMFS_NAME_LENGTH = 32;

    struct RamfsEntry {
        char name[MAX_RAMFS_NAME_LENGTH];
        RamfsFile file;
        bool used;
    };

    RamfsEntry entries[MAX_RAMFS_FILES];

    bool names_equal(const char* first, const char* second) {
        for (size_t index = 0; ; index++) {
            if (first[index] != second[index]) {
                return false;
            }
            if (first[index] == '\0') {
                return true;
            }
        }
    }

    bool copy_name(char* destination, const char* source) {
        size_t index = 0;
        while (source[index] != '\0') {
            if (index + 1 >= MAX_RAMFS_NAME_LENGTH) {
                return false;
            }
            destination[index] = source[index];
            index++;
        }
        destination[index] = '\0';
        return true;
    }
}

extern "C" void ramfs_init() {
    for (size_t index = 0; index < MAX_RAMFS_FILES; index++) {
        entries[index].used = false;
    }
}

extern "C" bool ramfs_register_file(const char* name, const void* data, size_t size) {
    if (!name || !data || size == 0) {
        return false;
    }

    for (size_t index = 0; index < MAX_RAMFS_FILES; index++) {
        if (entries[index].used && names_equal(entries[index].name, name)) {
            return false;
        }
    }

    for (size_t index = 0; index < MAX_RAMFS_FILES; index++) {
        if (!entries[index].used && copy_name(entries[index].name, name)) {
            entries[index].file.data = reinterpret_cast<const char*>(data);
            entries[index].file.size = size;
            entries[index].used = true;
            return true;
        }
    }

    return false;
}

extern "C" const RamfsFile* ramfs_open(const char* name) {
    if (!name) {
        return nullptr;
    }

    for (size_t index = 0; index < MAX_RAMFS_FILES; index++) {
        if (entries[index].used && names_equal(entries[index].name, name)) {
            return &entries[index].file;
        }
    }

    return nullptr;
}
