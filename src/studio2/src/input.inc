; input.inc -- the two keypads, merged (either pad plays): a bit a key.
;
; OUT 2 latches a key number (0-9, the low nibble of M(R(X))) into the
; console's CD4515; EF3 then reads that key on pad A, EF4 on pad B. OUT is a
; memory READ of M(R(X)), so R(X) points at console RAM (KTMP), never at the
; cart.
;
; KHELD: bit k (k 0-7 in the low byte, 8-9 in the high) set while key k is
; held on either pad. KNEW: the ones pressed since a screen took them (it
; clears them). KEV: the lowest key held, when it is pressed and then as it
; repeats (menus, the name, the shop); NONE once taken.

KEY_DELAY       EQU     24      ; jiffies before a held key repeats
KEY_RATE        EQU     6       ; jiffies between repeats

; in_poll: every pass. Clobbers R8, R9, RB.0, RC.
        RFIT    175
in_poll:
        LDR     R9, KTMP
        LDI     NONE
        PLO     RB                      ; the lowest key held
        LDI     0
        PLO     R8                      ; keys 0-7 held
        PHI     R8                      ; keys 8-9
        PLO     RC                      ; the key
        LDI     1
        PHI     RC                      ; its bit
inp1:   GLO     RC
        STR     R9
        SEX     R9
        OUT     2
        DEC     R9
        SEX     R2
        B3      inp2
        BN4     inp3
inp2:   GLO     RB                      ; held: the lowest?
        XRI     NONE
        BNZ     inp2a
        GLO     RC
        PLO     RB
inp2a:  GLO     RC                      ; its bit
        ANI     8
        BNZ     inp2h
        GHI     RC
        STR     R2
        GLO     R8
        OR
        PLO     R8
        BR      inp3
inp2h:  GHI     RC
        STR     R2
        GHI     R8
        OR
        PHI     R8
inp3:   INC     RC                      ; the next key, the next bit
        GHI     RC
        SHL
        BNZ     inp4
        LDI     1                       ; (keys 8-9: from bit 0 again)
inp4:   PHI     RC
        GLO     RC
        XRI     10
        BNZ     inp1
        VAR     KHELD                   ; the new presses: held & ~held before
        LDN     RA
        XRI     0FFH
        STR     R2
        GLO     R8
        AND
        STR     R2
        VAR     KNEW
        LDN     RA
        OR
        STR     RA
        VAR     KHELD+1
        LDN     RA
        XRI     0FFH
        STR     R2
        GHI     R8
        AND
        STR     R2
        VAR     KNEW+1
        LDN     RA
        OR
        STR     RA
        GLO     R8
        PUTV    KHELD
        GHI     R8
        PUTV    KHELD+1
        GETV    KLAST                   ; KEV: a new lowest key at once,
        STR     R2                      ;  a held one after KEY_DELAY, then
        GLO     RB                      ;  every KEY_RATE
        XOR
        BZ      inp6
        GLO     RB
        PUTV    KLAST
        XRI     NONE
        BZ      inp9
        LDI     KEY_DELAY
        BR      inp7
inp6:   GLO     RB
        XRI     NONE
        BZ      inp9
        GETV    KREPT                   ; JIFFY - KREPT, signed: not yet?
        STR     R2
        GETV    JIFFY
        SM
        ANI     80H
        BNZ     inp9
        LDI     KEY_RATE
inp7:   STR     R2
        GETV    JIFFY
        ADD
        PUTV    KREPT
        GLO     RB
        PUTV    KEV
inp9:   FRET

; key_take: D = KEV (NONE: none), now taken.
        RFIT    10
key_take:
        GETV    KEV
        PHI     RD
        LDI     NONE
        STR     RA
        GHI     RD
        FRET
