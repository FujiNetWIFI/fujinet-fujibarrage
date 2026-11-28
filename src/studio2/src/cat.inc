; cat.inc -- the CATALOG (docs/PROTOCOL.md), as K frames brought it into
; CATBUF, indexed once it is whole (src/cat.c cat_check; the Channel F's
; cat.inc).
;
;   version, items (48), settings (27), pages (n), the pages' names, then
;   items: kind, arms, bundle, price24, name; settings: page, type, label,
;   then (type 0) n, names, or (1) min16, step16, count, decimals; tiers:
;   n, names. Names are length-prefixed.
;
; The index (CATIDX, the cart's RAM: written by one poke stream as the walk
; goes) holds each record's address, big-endian: PAGEAT (4), ITEMAT (48),
; SETAT (27), TIERAT (1).

PAGEAT  EQU     CATIDX
ITEMAT  EQU     CATIDX+2*4
SETAT   EQU     ITEMAT+2*N_ITEMS
TIERAT  EQU     SETAT+2*N_SETTINGS
CATIDXN EQU     TIERAT+2-CATIDX

; cat_check: D = 1 if the catalog is whole and indexed (CATOK), else 0.
        RFIT    59
cat_check:
        GETV    CATOK
        BZ      cc0
        LDI     1
        FRET
cc0:    GETS    OFF_CAT_LEN             ; its length (LE) said, and all here
        PLO     R9
        INC     RB
        LDN     RB
        PHI     R9
        GLO     R9
        BNZ     cc1
        GHI     R9
        BZ      ccno
cc1:    GETV    CATN
        STR     R2
        GLO     R9
        XOR
        BNZ     ccno
        GETV    CATN+1
        STR     R2
        GHI     R9
        XOR
        BNZ     ccno
        FCALL   cat_index
        SETV    CATOK, 1
        LDI     1
        FRET
ccno:   LDI     0
        FRET

; cat_index: the walk, the index poked as it goes.
        RFIT    176
cat_index:
        POKEAT  CATIDX
        LDR     R8, CATBUF+3            ; the pages: up to 4 (the rest 0)
        LDN     R8
        INC     R8
        PLO     R9
        LDI     4
        PHI     R9
ci1:    GLO     R9
        BZ      ci2
        DEC     R9
        GHI     R8                      ; its address
        PKD
        GLO     R8
        PKD
        LDA     R8                      ; its name skipped
        FCALL   cat_skip
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     ci1
ci2b:   GLO     R9                      ; (more than 4: the rest skipped)
        BZ      ci3
        DEC     R9
        LDA     R8
        FCALL   cat_skip
        BR      ci2b
ci2:    GHI     R9                      ; (fewer pages: zeros)
        BZ      ci3
        SMI     1
        PHI     R9
        LDI     0
        PKD
        LDI     0
        PKD
        BR      ci2
ci3:    LDI     N_ITEMS                 ; the items: 6 bytes, the name
        PLO     R9
ci4:    GHI     R8
        PKD
        GLO     R8
        PKD
        GLO     R8
        ADI     6
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        LDA     R8
        FCALL   cat_skip
        DEC     R9
        GLO     R9
        BNZ     ci4
        LDI     N_SETTINGS              ; the settings
        PLO     R9
ci5:    GHI     R8
        PKD
        GLO     R8
        PKD
        INC     R8                      ; page
        LDA     R8                      ; type
        PHI     R9
        LDA     R8                      ; label
        FCALL   cat_skip
        GHI     R9
        BNZ     ci7
        LDA     R8                      ; a choice: n names
        PHI     R9
ci6:    GHI     R9
        BZ      ci8
        SMI     1
        PHI     R9
        LDA     R8
        FCALL   cat_skip
        BR      ci6
ci7:    GLO     R8                      ; a number: min16 step16 count decimals
        ADI     6
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
ci8:    DEC     R9
        GLO     R9
        BNZ     ci5
        GHI     R8                      ; the tiers
        PKD
        GLO     R8
        PKD
        PKEND
        FRET

; cat_skip: R8 += D (a name's length, after its length byte). Keeps R9,
;   the poke stream.
        RFIT    9
cat_skip:
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        FRET

; cat_at: R8 = the address the index holds at RB (ITEMAT + 2i ...).
        RFIT    5
cat_at: LDA     RB
        PHI     R8
        LDN     RB
        PLO     R8
        FRET

; cat_item: R8 = item D's record (kind, arms, bundle, price24, name).
        RFIT    12
cat_item:
        SHL
        ADI     LO(ITEMAT)
        PLO     RB
        LDI     HI(ITEMAT)
        ADCI    0
        PHI     RB
        FJMP    cat_at

; cat_setting: R8 = setting D's record (page, type, label, then n names or
;   min16 step16 count decimals).
        RFIT    12
cat_setting:
        SHL
        ADI     LO(SETAT)
        PLO     RB
        LDI     HI(SETAT)
        ADCI    0
        PHI     RB
        FJMP    cat_at

; cat_page: R8 = page D's name (length-prefixed; 4 pages at most).
        RFIT    14
cat_page:
        ANI     3
        SHL
        ADI     LO(PAGEAT)
        PLO     RB
        LDI     HI(PAGEAT)
        ADCI    0
        PHI     RB
        FJMP    cat_at

; cat_tier: R8 = tier D's name (the first if past the last).
        RFIT    34
cat_tier:
        PLO     R9
        LDR     RB, TIERAT
        FCALL   cat_at                  ; n, names
        LDA     R8
        STR     R2
        GLO     R9
        SD                              ; (n - t: none past)
        BZ      ctr0
        BNF     ctr0
ctr1:   GLO     R9
        BZ      ctr9
        DEC     R9
        LDA     R8
        FCALL   cat_skip
        BR      ctr1
ctr0:   LDI     0
        PLO     R9
        BR      ctr1
ctr9:   FRET

; cat_lab: R8 past setting D's page, type and label: on its n (a choice) or
;   its min16 (a number); D = its type.
        RFIT    12
cat_lab:
        FCALL   cat_setting
        INC     R8
        LDA     R8
        PHI     R9                      ; (the type)
        LDA     R8
        FCALL   cat_skip
        GHI     R9
        FRET

; cat_count: D = how many values setting D has.
        RFIT    11
cat_count:
        FCALL   cat_lab
        BZ      ccn1
        INC     R8                      ; a number: its count (after min, step)
        INC     R8
        INC     R8
        INC     R8
ccn1:   LDN     R8
        FRET

; cat_value: setting T4 at value T5 as text in VALBUF (NUL-ended): a
;   choice's name, or min + v step with its decimals.
        RFIT    68
cat_value:
        GETV    T4
        FCALL   cat_lab
        BNZ     cvl5
        LDA     R8                      ; a choice: past v names (n - 1 at most)
        PLO     RC
        GETV    T5
        PLO     R9
        STR     R2
        GLO     RC
        SM
        BZ      cvl0
        BDF     cvl1
cvl0:   GLO     RC
        SMI     1
        PLO     R9
cvl1:   GLO     R9
        BZ      cvl2
        DEC     R9
        LDA     R8
        FCALL   cat_skip
        BR      cvl1
cvl2:   LDA     R8                      ; its name, 11 at most
        PLO     RC
        SMI     12
        BNF     cvl3
        LDI     11
        PLO     RC
cvl3:   LDR     R9, VALBUF
cvl4:   GLO     RC
        BZ      cvl4a
        DEC     RC
        LDA     R8
        STR     R9
        INC     R9
        BR      cvl4
cvl4a:  LDI     0
        STR     R9
        FRET
cvl5:   FJMP    cat_num

; cat_num: R8 on a number setting's min16 step16 count decimals: min + T5
;   step (24-bit) as text in VALBUF, its decimals put in.
        RFIT    126
cat_num:
        LDA     R8                      ; T0-T2 = min
        PUTV    T0
        LDA     R8
        PUTV    T1
        SETV    T2, 0
        LDA     R8                      ; RC = step
        PLO     RC
        LDA     R8
        PHI     RC
        INC     R8
        LDN     R8                      ; T6 = decimals
        PUTV    T6
        GETV    T5                      ; + v step: v's bits, step shifted
        PLO     R9
        LDI     0
        PHI     R9                      ; (step's third byte)
        LDI     8
        PUTV    T3
cnm1:   GLO     R9
        SHR
        PLO     R9
        BNF     cnm2
        GLO     RC
        STR     R2
        GETV    T0
        ADD
        STR     RA
        GHI     RC
        STR     R2
        GETV    T1
        ADC
        STR     RA
        GHI     R9
        STR     R2
        GETV    T2
        ADC
        STR     RA
cnm2:   GLO     RC
        SHL
        PLO     RC
        GHI     RC
        SHLC
        PHI     RC
        GHI     R9
        SHLC
        PHI     R9
        GETV    T3
        SMI     1
        STR     RA
        BNZ     cnm1
        SETV    T3, 0                   ; (a fourth byte: 0)
        LDR     R8, T0
        LDR     R9, VALBUF
        FCALL   num32                   ; its digits
        GETV    T6
        BZ      cnm9
        FJMP    cat_dec
cnm9:   FRET

; cat_dec: T6 decimals put into VALBUF's number (0.20, 1.5): zeros in
;   front until there are more digits than decimals, then the point.
        RFIT    82
cat_dec:
        LDR     R8, VALBUF              ; its length
        LDI     0
        PLO     RC
cdc1:   LDA     R8
        BZ      cdc2
        INC     RC
        BR      cdc1
cdc2:   GETV    T6                      ; len <= dec: a zero in front
        STR     R2
        GLO     RC
        SD
        BNF     cdc4
        LDR     R8, VALBUF+11           ; (shift right one, a '0' first)
        LDR     R9, VALBUF+10
cdc3:   LDN     R9
        STR     R8
        DEC     R8
        DEC     R9
        GLO     R8
        XRI     LO(VALBUF)
        BNZ     cdc3
        LDI     '0'
        STR     R8
        INC     RC
        BR      cdc2
cdc4:   GETV    T6                      ; the point before the last dec digits
        STR     R2                      ;  (shift them right one)
        GLO     RC
        SM
        ADI     LO(VALBUF)
        PLO     R9                      ; R9: where the point goes
        LDR     R8, VALBUF+11
cdc5:   DEC     R8
        LDN     R8
        INC     R8
        STR     R8
        DEC     R8
        GLO     R8
        STR     R2
        GLO     R9
        XOR
        BNZ     cdc5
        LDI     '.'
        STR     R8
        FRET

; num32: the 32-bit number at R8 (LE, console RAM; it is spent) in decimal
;   at R9 (NUL-ended, no zeros in front). Clobbers RB, RC, T7.
        RFIT    103
num32:  LDR     RB, P10_32              ; 10^9 .. 10^0, LE
        LDI     10
        PUTV    T7
        LDI     0
        PHI     RC                      ; (a digit written yet)
n321:   LDI     0
        PLO     RC                      ; this digit
n322:   LDA     RB                      ; value - 10^k (each byte of the
        STR     R2                      ;  power staged at M(R2))
        LDA     R8
        SM
        PHI     RD                      ; r0
        LDA     RB
        STR     R2
        LDA     R8
        SMB
        PLO     RF                      ; r1 (RF: free here, parked after)
        LDA     RB
        STR     R2
        LDA     R8
        SMB
        PHI     RF                      ; r2
        LDA     RB
        STR     R2
        LDN     R8                      ; (R8 on v3)
        SMB                             ; r3
        DEC     RB
        DEC     RB
        DEC     RB
        DEC     RB
        BNF     n323a
        STR     R8                      ; (taken: the new value, v3 down)
        DEC     R8
        GHI     RF
        STR     R8
        DEC     R8
        GLO     RF
        STR     R8
        DEC     R8
        GHI     RD
        STR     R8
        INC     RC
        BR      n322
n323a:  DEC     R8                      ; (R8 back on v0)
        DEC     R8
        DEC     R8
n323:   LDI     FN_PARK
        PHI     RF
        GLO     RC                      ; a digit, unless a zero in front
        BNZ     n324                    ;  (the last one always)
        GHI     RC
        BNZ     n324
        GETV    T7
        XRI     1
        BNZ     n325
n324:   GLO     RC
        ADI     '0'
        STR     R9
        INC     R9
        LDI     1
        PHI     RC
n325:   INC     RB                      ; the next power
        INC     RB
        INC     RB
        INC     RB
        GETV    T7
        SMI     1
        STR     RA
        BNZ     n321
        LDI     0
        STR     R9
        FRET

        PFIT    40
P10_32: DB      000H, 0CAH, 09AH, 03BH, 000H, 0E1H, 0F5H, 005H, 080H, 096H
        DB      098H, 000H, 040H, 042H, 00FH, 000H, 0A0H, 086H, 001H, 000H
        DB      010H, 027H, 000H, 000H, 0E8H, 003H, 000H, 000H, 064H, 000H
        DB      000H, 000H, 00AH, 000H, 000H, 000H, 001H, 000H, 000H, 000H
