#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <kenv.h>
#include <ps5/kernel.h>
#include <ps5/payload.h>
#include "utils.h"
#define HV_ESCAPE(x)
#include "trampoline/hv_escape.h"

uint64_t kread64(uint64_t addr)
{
    uint64_t value;
    kernel_copyout(addr, &value, sizeof(value));
    return value;
}

void kwrite64(uint64_t addr, uint64_t value)
{
    kernel_copyin(&value, addr, sizeof(value));
}

uint64_t virt2phys(uint64_t va, uint64_t* pagesize)
{
    uint64_t pml = get_cr3();
    for(size_t i = 39; i >= 12; i -= 9)
    {
        uint64_t pmle = kread64(get_dmap_base() + pml + 8 * ((va >> i) & 511));
        *pagesize = ((uint64_t)1 << i) - (va & (((uint64_t)1 << i) - 1));
        if(!(pmle & 1))
            return -1;
        else if(i == 12 || (pmle & 0x80))
            return pmle & (((uint64_t)1 << 52) - ((uint64_t)1 << i));
        pml = pmle & (((uint64_t)1 << 52) - ((uint64_t)1 << 12));
    }
    __builtin_assume(false);
}

void leak_fd(int fd)
{
    uint64_t filedesc = kernel_get_proc_filedesc(getpid());
    uint64_t ofiles = kread64(filedesc);
    uint64_t file = kread64(ofiles + 48 * (uint64_t)fd + 8);
    uint32_t refcount;
    kernel_copyout(file+40, &refcount, sizeof(refcount));
    refcount++;
    kernel_copyin(&refcount, file+40, sizeof(refcount));
}

static uint64_t get_pmap(void)
{
    uint64_t proc = kernel_get_proc(getpid());
    uint64_t vmspace = kread64(proc + KERNEL_OFFSET_PROC_P_VMSPACE);
    return kread64(vmspace + 0x1d0);
}

uint64_t get_dmap_base(void)
{
    static uint64_t dmap;
    if(!dmap)
    {
        uint64_t pmap = get_pmap();
        dmap = kread64(pmap+32) - kread64(pmap+40);
    }
    return dmap;
}

uint64_t get_cr3(void)
{
    static uint64_t cr3;
    if(!cr3)
        cr3 = kread64(get_pmap()+40);
    return cr3;
}

//XXX: 4.03 offsets

uint64_t get_acpigbl_facs(void)
{
    return KERNEL_ADDRESS_DATA_BASE + 0x2d27eb0;
}

uint64_t get_idt(void)
{
    return KERNEL_ADDRESS_DATA_BASE + 0x64cdc80;
}

uint64_t get_tss(void)
{
    return KERNEL_ADDRESS_DATA_BASE + 0x64d0830;
}

uint64_t get_add_rsp_0xe8_iret(void)
{
    return KERNEL_ADDRESS_DATA_BASE - 0x9cf853;
}

uint64_t get_justreturn_pop(void)
{
    return KERNEL_ADDRESS_DATA_BASE - 0x9cf988;
}

uint64_t get_mov_cr3_rax(void)
{
    return KERNEL_ADDRESS_DATA_BASE - 0x9d67e9;
}

uint64_t get_acpi_rsdp(void)
{
    char buf[19];
    if(kenv(KENV_GET, "acpi.rsdp", buf, sizeof(buf)) != sizeof(buf))
        return 0;
    unsigned long long ans = 0;
    sscanf(buf, "%llx", &ans);
    return ans;
}

void get_bsd_kernel_range(uint64_t* start, uint64_t* end)
{
    uint64_t start_va = KERNEL_ADDRESS_TEXT_BASE;
    uint64_t end_va = get_idt() & -4096;
    uint64_t pagesize;
    uint64_t start_pa = virt2phys(start_va, &pagesize);
    for(uint64_t i = start_va + pagesize; i < end_va; i += pagesize)
    {
        uint64_t pa = virt2phys(i, &pagesize);
        if(pa != start_pa + i - start_va)
        {
            notify("get_bsd_kernel_range: kernel not physically contigous");
            payload_exit(1);
        }
    }
    *start = start_pa;
    *end = start_pa + end_va - start_va;
}

bool have_hv_exploit(void)
{
    return hv_escape(kernel_get_fw_version());
}

int sceKernelSendNotificationRequest(int, void*, size_t, int);

void notify(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    char msg[3120] = {};
    vsnprintf(msg+45, 3075, fmt, va);
    sceKernelSendNotificationRequest(0, msg, sizeof(msg), 0);
}

void string_append(char** s, const char* s2)
{
    *s = realloc(*s, strlen(*s) + strlen(s2) + 1);
    strcat(*s, s2);
}

void trim_newlines(char* s)
{
    size_t i = strlen(s);
    while(i && s[i-1] == '\n')
        s[--i] = 0;
}

int sceKernelOpenEventFlag(void**, const char*);
int sceKernelNotifySystemSuspendStart(void);
int sceKernelSetEventFlag(void*, int);
int sceKernelCloseEventFlag(void*);

void enter_rest_mode(void)
{
    kernel_copyin("\0\0\0", get_dmap_base()+0xc0115110, 4);
    kernel_copyin("", KERNEL_ADDRESS_DATA_BASE+0x13522a8, 1);
    void* event = 0;
    sceKernelOpenEventFlag(&event, "SceSystemStateMgrStatus");
    sceKernelNotifySystemSuspendStart();
    sceKernelSetEventFlag(event, 0x400);
    sceKernelCloseEventFlag(event);
}
