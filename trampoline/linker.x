SECTIONS
{
    . = 0;
    .text : {
        *(.text.entry*);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.data)
        *(.data.*)
        *(.bss)
        *(.bss.*)
    }

    .spinloop : {
        *(.spinloop)
    }
}
