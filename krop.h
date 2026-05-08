#pragma once
#include <stdint.h>

void setup_krop(uint64_t addr, uint16_t cs, uint64_t rcx, uint64_t rdx, uint64_t* pagetable_start, uint64_t* pagetable_end);
