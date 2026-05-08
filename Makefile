all: payload.elf

clean:
	rm -f payload.elf
	cd trampoline; make clean

trampoline/%.bin: trampoline/*.S trampoline/*.c trampoline/*.h bootparams.h
	cd trampoline; make

payload.elf: *.c *.h trampoline/payload.bin trampoline/spinloop.bin trampoline/hv_escape.h trampoline/trampoline_params.h
	prospero-clang --std=gnu23 -ffunction-sections -fdata-sections -Wl,--gc-sections *.c -lkernel_sys -o payload.elf
