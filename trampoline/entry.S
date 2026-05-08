.intel_syntax noprefix
.section .text.entry
.global _start
.global escaped_hv
.global trampoline_params
.global memset

#define rel rip+
#define byte byte ptr
#define dword dword ptr
#define qword qword ptr

//ABI: rax = our cr3, rcx = kernel start + zero page, rdx = params
_start:
cld

mov qword [rel our_cr3], rax
mov qword [rel zero_page], rcx
mov qword [rel trampoline_params], rdx

//acknowledge the APIC interrupt
mov eax, 0xfee00000
mov dword [rax+0xb0], 0

lock inc dword [rel cpu_barrier]
cmp dword [rel cpu_barrier], 1
jnz .not_first_cpu

//first CPU to reach here brings up other CPUs, performs cleanups & optionally sets up HV escape
//this calls functions from the kernel, so we have to use kernel stack
//FIXME: 4.03 offset?
mov rsp, gs:[0x720]
mov eax, 11
cpuid
mov edi, edx
call global_cleanups

.not_first_cpu:

//barrier
lock inc dword [rel cpu_barrier]
1:
cmp dword [rel cpu_barrier], 32
jne 1b

//vmmcall, rax = 4
//if HV is vulnerable, this exploits it and jumps to the HV path
//if HV is not vulnerable, this is VMMCALL_SET_CPUID_PPR
mov eax, 4
vmmcall

//barrier (for symmetry with hv path)
lock inc dword [rel cpu_barrier]
1:
cmp dword [rel cpu_barrier], 48
jne 1b

//if we're here, boot in GameOS VM mode
mov r15d, 0 //gameos vm mode
jmp boot_linux

.section .text.escaped_hv

//if we're here, HV was successfully exploited
escaped_hv:
//barrier
lock inc dword [rel cpu_barrier]
1:
cmp dword [rel cpu_barrier], 48
jne 1b

//cleanups from ps5-linux-loader

cli
stgi

//clear EFER.SVM
mov ecx, 0xc0000080
rdmsr
btc eax, 12
wrmsr

//disable INIT redirection
mov ecx, 0xc0010114
rdmsr
btc eax, 1
wrmsr

//end cleanups

mov r15d, 1 //hv mode

boot_linux:

//load GDT
lea rax, [rel linux_gdt]
mov [rel linux_gdtr+2], rax
lgdt [rel linux_gdtr]

//load cs. this is tricky because we have no actual stack here, but every core loads the same thing so we should be ok
lea rsp, [rel c_stack]
push dword 0x18
push dword 0
push dword 2
push dword 0x10
lea rax, [rel 1f]
push rax
iretq
1:

//load the rest of the segments
mov ax, ss
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

//barrier
lock inc dword [rel cpu_barrier]
1:
cmp dword [rel cpu_barrier], 64
jne 1b

//get our CPU ID
mov eax, 11
cpuid
test edx, edx
jne .hangup

//wait for other cores to enter the wait loop
1:
cmp dword [rel cpu_barrier], 79
jne 1b

//run cleanups that require defeated HV
test r15d, r15d
jz 1f
lea rsp, [rel c_stack]
call cleanups_hv
1:

//jump of faith
mov rax, [rel linux_entry]
mov rsi, [rel zero_page]
xor ebp, ebp
xor edi, edi
xor ebx, ebx
jmp rax

.hangup:
test r15d, r15d
jz .vm_hangup

//place an infinite loop inside hv memory and jump to
//this memory is reserved from linux anyway
mov rax, 0x62820000
lea rcx, [rel cpu_barrier]
mov rdx, 0xfeeb01fff0 //lock inc dword [rcx]; 1: jmp 1b
mov [rax], rdx
jmp rax

.vm_hangup:
mov rax, [rel trampoline_params]
mov rax, [rax]
lea rcx, [rel cpu_barrier]
jmp rax //to spinloop+2; see acpi.c

.section .text.memset

memset:
mov rax, rsi
mov rsi, rdi
mov rcx, rdx
rep stosb
mov rax, rsi
ret

.section .spinloop

spinloop:
//skip counter increment
jmp 1f
lock inc dword [rcx]
1:

//get our cpu id
mov eax, 11
cpuid
shl rdx, 32
or rdx, 1

//wait for the mailbox command from linux
1:
cmp qword [rel spinloop-2048], rdx
jne 1b

//acknowledge the command
mov rdi, [rel spinloop-2040]
mov byte [rel spinloop-2048], 0

//jump of faith
xor eax, eax
xor ecx, ecx
xor edx, edx
jmp rdi

.section .data.descriptors

.align 8

linux_gdtr:
.8byte 31
.8byte 0

linux_gdt:
.8byte 0
.8byte 0
.8byte 0x00af9a000000ffff
.8byte 0x00cf92000000ffff

.section .data.trampoline_params

trampoline_params:
.8byte 0

.section .data.stack

.p2align 12
.long 0
.p2align 12
c_stack:
