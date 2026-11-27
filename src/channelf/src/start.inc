; ---------------------------------------------------------------------------
; start.inc -- power-up, the pages, the clock (FujiBlast's src/channelf
; start.inc: M.U.L.E.'s, cut to what this game needs).
; ---------------------------------------------------------------------------

; ---------------------------------------------------------------------------
; START -- the BIOS jumps here ($0802) at power-up and on RESET. It has
; zeroed the scratchpad and set r59 = 40; RAM is the cart's and outlives a
; RESET, so everything is set here: the globals zeroed, the player's page
; and the data page copied into RAM (they run and are read from there).
; ---------------------------------------------------------------------------
START:  DI
        LISU 7                  ; r59 = KSTK0: the K-stack empty
        LISL 3
        LI KSTK0
        LR S,A
        DCI VARS                ; the globals and the tables after them: 0
        LI (RAMEND-VARS+255)>>8
        LR 1,A
        LI (RAMEND-VARS)&0FFH
        LR 0,A
STZL:   CLR
        ST
        DS 0
        BNZ STZL
        DS 1
        BNZ STZL
        PI FNCHK                ; no cart answering: no pages either
        BZ STOK0
        JMP NOCART
STOK0:  LDB FNPVER
        CI 1
        BNC STOK1               ; protocol 2 or later
        JMP OLDCART
STOK1:  LIS PG_PLY              ; the player into RAM
        LR 9,A
        PI SELPG
        DCI PGWIN
        XDC
        DCI PLYRAM
        XDC
        LI PGSIZE>>8
        LR 1,A
        PI STCOPY
        LIS PG_DATA             ; the sounds into RAM
        LR 9,A
        PI SELPG
        DCI PGWIN
        XDC
        DCI DATA
        XDC
        LI DATASZ>>8
        LR 1,A
        PI STCOPY
        LIS PG_BOOT
        LR 9,A
        PI SELPG
        PI SNDINIT
        PI DINIT
        JMP MAIN

; STCOPY -- r1 x 256 bytes from DC0 to DC1.  Leaf.  Clobbers A, r0, r1, DCs.
STCOPY: CLR
        LR 0,A
STCPL:  LM
        XDC
        ST
        XDC
        DS 0
        BNZ STCPL
        DS 1
        BNZ STCPL
        POP

; No FujiNet cart answering, or one too old to page: say so and stop.
NOCART: PI STOP
        DCI S_NOCART
        BR HALT
OLDCART: PI STOP
        DCI S_OLDCART
HALT:   PI TXSTR
HALT1:  BR HALT1
; STOP -- a clear screen, text at row 3 in white (palette 0).
STOP:   ENTER
        PI DINIT
        LIS 1
        LR 0,A
        LIS 3
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        LEAVE
S_NOCART: DB "NO FUJINET FOUND",0
S_OLDCART: DB "CART FIRMWARE TOO OLD",0

; FNCHK -- Z set if the cart's 'F','N' magic is there.  Leaf; clobbers A, DC0.
FNCHK:  DCI FNMAG0
        LM
        CI 'F'
        BNZ FNCHKX
        LM
        CI 'N'
FNCHKX: POP

; ---------------------------------------------------------------------------
; SELPG -- map page r9 at $4800.  Leaf; keeps DC0.  Clobbers A, DC1.
; With PGPUSH, the only stores to FNBANK (tools/chf/checkcalls.py).
; ---------------------------------------------------------------------------
SELPG:  XDC
        LR A,9
        DCI CURPG
        ST
        DCI FNBANK
        ADC
        ST                      ; any value: the page is mapped from here on
        XDC
        POP

; PGPUSH -- remember the page mapped now, then map page r9.  Leaf.
;   Clobbers A, r8, DC0.
PGPUSH: DCI CURPG
        LM
        LR 8,A                  ; the page now
        LM                      ; (CURPG, PGSP, PGSTK follow each other)
        ADC                     ; DC0 = PGSTK + depth
        LR A,8
        ST
        DCI PGSP
        LM
        INC
        DCI PGSP
        ST
        LR A,9
        DCI CURPG
        ST
        DCI FNBANK
        ADC
        ST
        POP

; PGPOP -- map the page PGPUSH remembered.  Leaf.  Clobbers A, r8, r9, DC0, DC1.
PGPOP:  DCI PGSP
        LM
        AI 0FFH
        LR 8,A
        DCI PGSP
        ST                      ; DC0 = PGSTK
        LR A,8
        ADC
        LM
        LR 9,A
        JMP SELPG               ; (its POP returns for us)

; FAR -- call ent, which is in page pg, from fixed code or the player in RAM
; (ENTER first), and come back to the page mapped before.  Results in r0-r6;
; A, r7-r9 and the DCs do not survive.  Never from a page.
FAR     MACRO pg, ent
        LIS pg
        LR 9,A
        PI PGPUSH
        PI PGWIN+(ent)
        PI PGPOP
        ENDM

; GOPAGE -- page pg's entry ent (E_*), from fixed code (ENTER first): the
; page stays mapped.  A comes back as the entry left it.
GOPAGE  MACRO pg, ent
        LIS pg
        LR 9,A
        PI SELPG
        PI PGWIN+(ent)
        ENDM

STUB:   POP

; ---------------------------------------------------------------------------
; TICKRD -- r0:r1 = the cart's tick (60 Hz).  The bytes change together but
;   are read one at a time: HI, LO, HI, again if HI moved.  Leaf.
;   Clobbers A, DC0.
; ---------------------------------------------------------------------------
TICKRD: DCI FNTICK+1
        LM
        LR 1,A
        DCI FNTICK
        LM
        LR 0,A
        LM
        XS 1
        BNZ TICKRD
        POP

; FRWAIT -- wait for the tick to move on from FRAMES; FRAMES = the tick.
;   Leaf.  Clobbers A, r0, r1, DC0.
FRWAIT: DCI FNTICK+1
        LM
        LR 1,A
        DCI FNTICK
        LM
        LR 0,A
        LM
        XS 1
        BNZ FRWAIT
        DCI FRAMES
        LM
        XS 0
        BNZ FRWNEW
        LM
        XS 1
        BZ FRWAIT
FRWNEW: DCI FRAMES
        LR A,0
        ST
        LR A,1
        ST
        POP

; ---------------------------------------------------------------------------
; FRAME -- wait for the cart's tick to move; DT = the jiffies since the last
;   pass (at most 8: a longer stall is not caught up), JIFFY += DT; the
;   input read; the sound's tick.
; ---------------------------------------------------------------------------
FRAME:  ENTER
FRM1:   PI TICKRD                       ; r0:r1 the tick
        DCI FRAMES
        LM
        COM
        INC
        AS 0
        LR 2,A                          ; DT (the low bytes do)
        NI 0FFH
        BZ FRM1
        LR A,2
        CI 8
        BC FRM2
        LIS 8
        LR 2,A
FRM2:   LR A,2
        STB DT
        DCI FRAMES
        LR A,0
        ST
        LR A,1
        ST
        DCI JIFFY
        LM
        AS 2
        LR 3,A
        LM
        LNK
        DCI JIFFY+1
        ST
        DCI JIFFY
        LR A,3
        ST
        PI INPOLL
        PI INTAKE
        PI SNDTICK
        LEAVE
