#pragma once

uint64_t kread64(uint64_t addr);
void kwrite64(uint64_t addr, uint64_t value);
uint64_t virt2phys(uint64_t va, uint64_t* pagesize);

uint64_t get_dmap_base(void);
uint64_t get_cr3(void);
uint64_t get_acpigbl_facs(void);
uint64_t get_idt(void);
uint64_t get_tss(void);
uint64_t get_add_rsp_0xe8_iret(void);
uint64_t get_justreturn_pop(void);
uint64_t get_mov_cr3_rax(void);
uint64_t get_acpi_rsdp(void);
void get_bsd_kernel_range(uint64_t* start, uint64_t* end);
bool have_hv_exploit(void);

void notify(const char*, ...);
void string_append(char**, const char*);
void trim_newlines(char* s);
void leak_fd(int fd);
void enter_rest_mode();
