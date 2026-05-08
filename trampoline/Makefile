all: payload.bin spinloop.bin

clean:
	rm -f payload.bin spinloop.bin payload.elf *.o

payload.bin: payload.elf
	objcopy -O binary --only-section .text $< $@

spinloop.bin: payload.elf
	objcopy -O binary --dump-section .spinloop=$@ $< /dev/null

%.o: %.S
	prospero-clang -ffunction-sections -fdata-sections -c $< -o $@

%.o: %.c *.h ../bootparams.h
	prospero-clang --std=gnu23 -c $< -o $@

payload.elf: entry.o cleanups.o hv_escape_tmr.o mp3.o iommu.o
	prospero-lld --gc-sections -pie -T linker.x $^ -o $@
	if grep -q '\.rela' $@; then false; fi
