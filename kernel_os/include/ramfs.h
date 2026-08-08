#pragma once
#include <stddef.h>
#include <stdint.h>

// ============================================================
// Read-only in-memory file system
// ============================================================

struct RamfsFile {
    const char* data;
    size_t size;
};

extern "C" {

void ramfs_init();
bool ramfs_register_file(const char* name, const void* data, size_t size);
const RamfsFile* ramfs_open(const char* name);

}
