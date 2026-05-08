#include <string.h>
#include <unistd.h>
#include <ps5/kernel.h>
#include "loader.h"
#include "utils.h"
#include "bootparams.h"
#include "trampoline/trampoline_params.h"
#include "e820.h"
#include "krop.h"
#include "acpi.h"

static inline bool validate_and_relocate_kernel(void** linux, size_t* linux_size, uint64_t* linux_phys)
{
    if(*linux_size < sizeof(struct boot_params))
        return false;
    struct boot_params* bp = *linux;
    memset(bp, 0, 0x1f1);
    if(bp->boot_flag != 0xaa55 || bp->header != 0x53726448 /* HdrS */ || bp->version < 0x0208)
        return false;
    size_t header_end = 0x202 + bp->jump[1];
    size_t setup_sects = bp->setup_sects + 1;
    if(setup_sects == 1)
        setup_sects = 5;
    size_t setup_size = 512 * setup_sects;
    if(*linux_size < setup_size)
        return false;
    memset((char*)*linux + header_end, 0, setup_size - header_end);
    //*linux_phys = to_physical_memory_partial(linux, setup_size + bp->init_size, *linux_size, bp->kernel_alignment, -setup_size, true);
    //we can't do the above because of memory shortage
    *linux_phys = to_physical_memory(linux, *linux_size, bp->kernel_alignment, -setup_size, true);
    return true;
}

static inline uint64_t prepare_trampoline_params(struct trampoline_params** tp, uint32_t vram_size)
{
    void* p;
    uint64_t phys = allocate_physical_memory(&p, sizeof(struct trampoline_params), 4096);
    struct trampoline_params* q = p;
    q->fw_version = kernel_get_fw_version();
    q->vram_size = vram_size;
    q->kdata_base = KERNEL_ADDRESS_DATA_BASE;
    *tp = q;
    return phys;
}

static constexpr uint8_t trampoline[] = {
    #embed "trampoline/payload.bin"
};

int main(void)
{
    notify("ps5-linuxldr2 loading Linux...");
    void* linux;
    size_t linux_size = read_file("bzImage", &linux, true);
    if((ssize_t)linux_size < 0)
        return 1;
    uint64_t linux_phys;
    if(!validate_and_relocate_kernel(&linux, &linux_size, &linux_phys))
    {
        notify("Kernel is invalid");
        return 1;
    }
    void* initrd;
    uint64_t initrd_phys;
    size_t initrd_size = read_file_to_phys("initrd.img", &initrd, true, &initrd_phys, 4096, 0);
    if((ssize_t)initrd_size < 0)
    {
        notify("warning: initrd not found, using empty");
        initrd_phys = 0;
        initrd_size = 0;
    }
    void* cmdline_f = 0;
    ssize_t cmdline_size = read_file("cmdline.txt", &cmdline_f, false);
    char* cmdline = cmdline_f;
    if(cmdline_size < 0)
        cmdline = strdup("root=/dev/sda2 rw rootwait console=ttyTitania0 console=tty0 video=DP-1:1920x1080@60 mitigations=off idle=halt pci=pcie_bus_perf");
    else
    {
        cmdline[cmdline_size] = 0;
        trim_newlines(cmdline);
    }
    struct boot_params* bp = linux;
    bp->ramdisk_image = initrd_phys;
    bp->ext_ramdisk_image = initrd_phys >> 32;
    bp->ramdisk_size = initrd_size;
    bp->ext_ramdisk_size = initrd_size >> 32;
    bp->type_of_loader = 13; //kexec
    bp->loadflags = 0;
    bp->hardware_subarch = 5; //X86_SUBARCH_PS5
    bp->acpi_rsdp = get_acpi_rsdp();
    e820_init(bp);
    e820_set_reserved(bp, 0x100000000, 0x140000000); //vram
    void* trampoline_copy = (void*)trampoline;
    uint64_t trampoline_phys = to_physical_memory(&trampoline_copy, sizeof(trampoline), 4096, 0, false);
    struct trampoline_params* tp;
    uint64_t tp_phys = prepare_trampoline_params(&tp, 1 << 30);
    uint64_t pagetable_start, pagetable_end;
    setup_krop(trampoline_phys, 0x20, linux_phys, tp_phys, &pagetable_start, &pagetable_end);
    if(!have_hv_exploit())
    {
        uint64_t bsd_kernel_start, bsd_kernel_end;
        get_bsd_kernel_range(&bsd_kernel_start, &bsd_kernel_end);
        e820_set_reserved(bp, bsd_kernel_start, bsd_kernel_end);
        e820_set_reserved(bp, pagetable_start, pagetable_end);
        gameos_patch_acpi(bp, tp);
        string_append(&cmdline, " disable_mtrr_trim");
    }
    cmdline_f = cmdline;
    uint64_t cmdline_phys = to_physical_memory(&cmdline_f, strlen(cmdline_f) + 1, 4096, 0, true);
    bp->cmd_line_ptr = cmdline_phys;
    bp->ext_cmd_line_ptr = cmdline_phys >> 32;
    bp->cmdline_size = strlen(cmdline_f);
    notify("Linux PA = 0x%lx (entry = 0x%lx)\ninitrd PA = 0x%lx\ncmdline PA = 0x%lx\nTrampoline PA = 0x%lx\nGoing to sleep...", linux_phys, linux_phys + 1024 + 512 * (bp->setup_sects?:4), initrd_phys, cmdline_phys, trampoline_phys);
    sleep(5);
    enter_rest_mode();
}
