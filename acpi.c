#include <stdlib.h>
#include <string.h>
#include <ps5/kernel.h>
#include "acpi.h"
#include "loader.h"
#include "utils.h"
#include "e820.h"

struct firmware_wakeup_table
{
    uint32_t command;
    uint32_t apic_id;
    uint64_t wakeup_vector;
    uint8_t os_reserved[2032];
    uint8_t firmware_reserved[2048];
};
_Static_assert(sizeof(struct firmware_wakeup_table) == 4096);

struct madt_firmware_wakeup
{
    uint8_t type;
    uint8_t length;
    uint16_t version;
    uint32_t reserved;
    uint64_t mailbox_address;
    uint64_t reset_vector;
};
_Static_assert(sizeof(struct madt_firmware_wakeup) == 24);

static constexpr uint8_t spinloop[] = {
    #embed "trampoline/spinloop.bin"
};
_Static_assert(sizeof(spinloop) < 2048 && spinloop[0] == 0xeb);

static uint64_t allocate_firmware_wakeup_table(struct boot_params* bp, struct trampoline_params* tp)
{
    void* va;
    uint64_t fwwt_pa = allocate_physical_memory(&va, 4096, 4096);
    struct firmware_wakeup_table* fwwt = va;
    memcpy(fwwt->firmware_reserved, spinloop, sizeof(spinloop));
    tp->firmware_wakeup = fwwt_pa + 2050; //skip the initial jump
    e820_set_acpi_reclaimable(bp, fwwt_pa, fwwt_pa + 4096);
    return fwwt_pa;
}

static void checksum(void* buf, size_t sz, size_t idx)
{
    uint8_t* b = buf;
    uint8_t cnt = 0;
    for(size_t i = 0; i < sz; i++)
        cnt += b[i];
    b[idx] -= cnt;
}

static uint64_t append_to_acpi_table(struct boot_params* bp, uint64_t base_pa, void* chk, size_t sz)
{
    uint32_t size;
    kernel_copyout(get_dmap_base() + base_pa + 4, &size, 4);
    void* table;
    uint64_t table_pa = allocate_physical_memory(&table, size + sz, 8);
    e820_set_acpi_reclaimable(bp, table_pa, table_pa + size + sz);
    kernel_copyout(get_dmap_base() + base_pa, table, size);
    size += sz;
    memcpy((char*)table + 4, &size, 4);
    memcpy((char*)table + (size - sz), chk, sz);
    checksum(table, size, 9);
    return table_pa;
}

struct xsdt
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oemrevision;
    uint32_t creatorid;
    uint32_t creatorrevision;
    uint64_t tables[];
} __attribute__((packed));

static struct xsdt* read_xsdt(uint64_t rsdp)
{
    uint64_t rsdt_pa;
    kernel_copyout(get_dmap_base() + rsdp + 24, &rsdt_pa, 8);
    uint32_t length;
    kernel_copyout(get_dmap_base() + rsdt_pa + 4, &length, 4);
    struct xsdt* ans = malloc(length);
    kernel_copyout(get_dmap_base() + rsdt_pa, ans, length);
    return ans;
}

static uint64_t write_xsdt(struct boot_params* bp, uint64_t rsdp, struct xsdt* xsdt)
{
    checksum(xsdt, xsdt->length, 9);
    void* p = xsdt;
    uint64_t xsdt_pa = to_physical_memory(&p, xsdt->length, 8, 0, false);
    e820_set_acpi_reclaimable(bp, xsdt_pa, xsdt_pa + xsdt->length);
    free(xsdt);
    uint8_t rsdp_data[36];
    kernel_copyout(get_dmap_base() + rsdp, rsdp_data, 36);
    memcpy(rsdp_data + 16, "\0\0\0", 4);
    checksum(rsdp_data, 20, 8);
    memcpy(rsdp_data + 24, &xsdt_pa, 8);
    checksum(rsdp_data, 36, 32);
    void* rsdp_ptr = rsdp_data;
    uint64_t new_rsdp = to_physical_memory(&rsdp_ptr, 36, 8, 0, false);
    e820_set_acpi_reclaimable(bp, new_rsdp, new_rsdp+36);
    return new_rsdp;
}

static uint64_t find_table(const struct xsdt* xsdt, const char* name)
{
    char nm[5];
    size_t n_tables = (xsdt->length - 36) / 8;
    for(size_t i = 0; i < n_tables; i++)
    {
        uint64_t table = xsdt->tables[i];
        kernel_copyout(get_dmap_base() + table, nm, 4);
        if(!strcmp(nm, name))
            return table;
    }
    return -1;
}

static void remove_table(struct xsdt* xsdt, uint64_t pa)
{
    size_t n_tables = (xsdt->length - 36) / 8;
    size_t j = 0;
    for(size_t i = 0; i < n_tables; i++)
        if(xsdt->tables[i] != pa)
            xsdt->tables[j++] = xsdt->tables[i];
    xsdt->length = 36 + 8 * j;
}

static void add_table(struct xsdt** xsdt, uint64_t pa)
{
    (*xsdt)->length += 8;
    *xsdt = realloc(*xsdt, (*xsdt)->length);
    (*xsdt)->tables[((*xsdt)->length - 36) / 8 - 1] = pa;
}

void gameos_patch_acpi(struct boot_params* bp, struct trampoline_params* tp)
{
    struct xsdt* xsdt = read_xsdt(bp->acpi_rsdp);
    remove_table(xsdt, find_table(xsdt, "IVRS"));
    uint64_t madt = find_table(xsdt, "APIC");
    remove_table(xsdt, madt);
    uint64_t fwwt = allocate_firmware_wakeup_table(bp, tp);
    struct madt_firmware_wakeup mfw = {
        .type = 16,
        .length = 24,
        .mailbox_address = fwwt,
        .reset_vector = fwwt + 2048,
    };
    madt = append_to_acpi_table(bp, madt, &mfw, sizeof(mfw));
    add_table(&xsdt, madt);
    bp->acpi_rsdp = write_xsdt(bp, bp->acpi_rsdp, xsdt);
}
