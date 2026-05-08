# ps5-linuxldr2

A loader for PS5 Linux that works in the GameOS VM.

## Building

`make`

This assumes that `$PS5SDK/bin` is in your PATH.

## Running

* Send this payload instead of ps5-linux-loader
* Build PS5 Linux with the provided patch
* Disable `CONFIG_X86_MCE`
* Don't even try to boot with mitigations enabled

Only firmware 4.03 is supported for now. The loader will boot outside the VM by default, to force it to boot in VM comment out `hv_escape_tmr` in `trampoline/hv_escape.h`.

## What works

* USB ports
* earlycon UART (`earlycon=titania`, separate patch, output-only)

## What does not work

* GPU
* Late UART
* Everything else
