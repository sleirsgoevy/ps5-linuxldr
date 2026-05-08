#pragma once
#include <stdint.h>

struct trampoline_params
{
    uint64_t firmware_wakeup; //must come first
    uint64_t vram_base;
    uint64_t vram_size;
    uint64_t kdata_base;
    uint32_t fw_version;
};

extern struct trampoline_params* trampoline_params;
