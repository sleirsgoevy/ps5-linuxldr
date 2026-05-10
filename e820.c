#include <stddef.h>
#include "e820.h"

enum
{
    TYPE_FREE = 1,
    TYPE_RESERVED = 2,
    TYPE_ACPI_RECLAIMABLE = 3,
    TYPE_NVS = 4,
};

static void e820_add_node(struct boot_params* bp, uint64_t start, uint64_t end, int type)
{
    struct e820_entry* e = &bp->entries[bp->e820_entries++];
    e->addr = start;
    e->size = end - start;
    e->type = type;
}

static void e820_cutout(struct boot_params* bp, uint64_t start, uint64_t end, int type)
{
    size_t j = 0;
    for(size_t i = 0; i < bp->e820_entries; i++)
    {
        uint64_t s = bp->entries[i].addr;
        uint64_t e = s + bp->entries[i].size;
        if(s >= start && e <= end)
            continue;
        else if(s < start)
        {
            if(e > end)
                e820_add_node(bp, end, e, bp->entries[i].type);
            if(e > start)
                bp->entries[i].size = start - s;
        }
        else if(e > end && s < end)
        {
            bp->entries[i].addr = end;
            bp->entries[i].size = e - end;
        }
        bp->entries[j++] = bp->entries[i];
    }
    bp->e820_entries = j;
    e820_add_node(bp, start, end, type);
}

int sceKernelIsDevKit(void);

void e820_init(struct boot_params* bp)
{
    //from ps5-linux-loader
    bp->e820_entries = 0;
    e820_add_node(bp, 0x000000000, 0x000001000, TYPE_RESERVED);
    e820_add_node(bp, 0x000001000, 0x000070000, TYPE_FREE);
    e820_add_node(bp, 0x000070000, 0x0000a0000, TYPE_RESERVED);
    e820_add_node(bp, 0x0000a0000, 0x0000c0000, TYPE_RESERVED);
    e820_add_node(bp, 0x0000c0000, 0x000100000, TYPE_RESERVED); // VBIOS
    e820_add_node(bp, 0x000100000, 0x03fffc000, TYPE_FREE);
    e820_add_node(bp, 0x03fffc000, 0x040000000, TYPE_RESERVED);
    e820_add_node(bp, 0x040000000, 0x060000000, TYPE_FREE);
    e820_add_node(bp, 0x060000000, 0x060800000, TYPE_RESERVED); // MP4
    e820_add_node(bp, 0x060800000, 0x060c00000, TYPE_RESERVED); // VCN FW
    e820_add_node(bp, 0x060c00000, 0x062800000, TYPE_FREE);
    e820_add_node(bp, 0x062800000, 0x064800000, TYPE_RESERVED); // HV
    e820_add_node(bp, 0x064800000, 0x064829000, TYPE_RESERVED); // MP3
    e820_add_node(bp, 0x064829000, 0x07f9d0000, TYPE_FREE);
    e820_add_node(bp, 0x07f9d0000, 0x07fd5f000, TYPE_RESERVED);
    e820_add_node(bp, 0x07fd5f000, 0x07fd63000, TYPE_RESERVED);
    e820_add_node(bp, 0x07fd63000, 0x07fd67000, TYPE_RESERVED);
    e820_add_node(bp, 0x07fd67000, 0x07fd6f000, TYPE_NVS);
    e820_add_node(bp, 0x07fd6f000, 0x07fd8f000, TYPE_ACPI_RECLAIMABLE);
    e820_add_node(bp, 0x07fd8f000, 0x07fd90000, TYPE_RESERVED);
    e820_add_node(bp, 0x07fd90000, 0x080000000, TYPE_RESERVED);
    e820_add_node(bp, 0x080000000, 0x0c4400000, TYPE_RESERVED);
    e820_add_node(bp, 0x0d0000000, 0x0e0700000, TYPE_RESERVED);
    e820_add_node(bp, 0x0f0000000, 0x0f8000000, TYPE_RESERVED);
    if(sceKernelIsDevKit())
    {
        e820_add_node(bp, 0x100000000, 0x87f300000, TYPE_FREE);
        e820_add_node(bp, 0x87f300000, 0x880000000, TYPE_RESERVED);
    }
    else //testkit/retail
    {
        e820_add_node(bp, 0x100000000, 0x47f300000, TYPE_FREE);
        e820_add_node(bp, 0x47f300000, 0x480000000, TYPE_RESERVED);
    }
}

void e820_set_reserved(struct boot_params* bp, uint64_t start, uint64_t end)
{
    e820_cutout(bp, start, end, TYPE_RESERVED);
}

void e820_set_acpi_reclaimable(struct boot_params* bp, uint64_t start, uint64_t end)
{
    e820_cutout(bp, start, end, TYPE_ACPI_RECLAIMABLE);
}
