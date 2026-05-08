#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

ssize_t read_file(const char* filename, void** out, bool allow_netcat);
uint64_t allocate_physical_memory(void** ptr, size_t sz, uint64_t alignment);
uint64_t to_physical_memory_partial(void** buf, size_t sz, size_t copy_sz, uint64_t alignment, uint64_t misalignment, bool move);
uint64_t to_physical_memory(void** buf, size_t sz, uint64_t alignment, uint64_t misalignment, bool move);
ssize_t read_file_to_phys(const char* filename, void** out, bool allow_netcat, uint64_t* out_phys, uint64_t alignment, uint64_t misalignment);
