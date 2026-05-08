#pragma once
#include "bootparams.h"
#include "trampoline/trampoline_params.h"

void gameos_patch_acpi(struct boot_params* bp, struct trampoline_params* tp);
