#pragma once
#include "bootparams.h"

void e820_init(struct boot_params* bp);
void e820_set_reserved(struct boot_params* bp, uint64_t start, uint64_t end);
void e820_set_acpi_reclaimable(struct boot_params* bp, uint64_t start, uint64_t end);
