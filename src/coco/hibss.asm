*
* hibss.asm -- the high core's variables (section hibss, $8000 up: the
* program's files don't hold them), and hibss_clear, which zeroes them once
* bank_init has the high RAM (the C start-up zeroes only the low bss, before
* there is any). Big buffers live here: the low core is the tight one.
*
        section hibss

        export  _scr_ch,_scr_inv
_scr_ch rmb     25*40           ; the text's cells (src/coco2/scr2.c, src/coco3/scr3.c)
_scr_inv rmb    25*5            ;  and which show inverse

        section code

        export  _hibss_clear
        import  s_hibss,l_hibss

_hibss_clear
        ldx     #s_hibss
        ldd     #l_hibss
        beq     @done
@zero   clr     ,x+
        subd    #1
        bne     @zero
@done   rts
