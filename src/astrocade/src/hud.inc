; hud.inc -- the HUD's row: the 3x5 font (FONT35, tools/astro/mkfont_astro.py)
; through the magic expander, 5 lines from line 0, 40 cells of 4 pixels.
; The HUD's colours are its own (VBISR: HUDC): colour 0 its ground, 1 its
; ink, left of byte 20 the turn's seat's, right of it a dark strip.

; TX5AT: the string at HL (NUL-ended, ASCII; lower case shows as upper) at
; column E, colour pair C. TX5N: exactly B cells (spaces pad). Clobber all
; but IX, IY.
TX5AT:  LD      B,0
TX5N:   LD      A,C
        OUT     (XPAND),A
        LD      D,0             ; DE: the magic address of line 0, column E
TX51:   LD      A,(HL)
        OR      A
        JR      Z,TX55
        INC     HL
        CALL    GLY5
        INC     E
        LD      A,B
        OR      A
        JR      Z,TX51
        DJNZ    TX51
        RET
TX55:   LD      A,B             ; pad
        OR      A
        RET     Z
TX56:   LD      A,' '
        CALL    GLY5
        INC     E
        DJNZ    TX56
        RET

; GLY5: character A at line 0, column E. Preserves BC, DE, HL.
GLY5:   PUSH    BC
        PUSH    DE
        PUSH    HL
        LD      D,0             ; (line 0: DE the magic address)
        CP      60H
        JR      C,G51
        SUB     20H             ; lower case -> upper case
G51:    SUB     20H
        CP      40H
        JR      C,G52
        LD      A,'?'-20H
G52:    LD      L,A             ; HL = FONT35 + 5A
        LD      H,0
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,BC
        LD      BC,FONT35
        ADD     HL,BC
        LD      B,HUDH
G53:    LD      A,MAGEXP
        OUT     (MAGIC),A
        LD      A,(HL)
        LD      (DE),A
        INC     HL
        LD      A,E
        ADD     A,BYTEPL
        LD      E,A
        JR      NC,G54
        INC     D
G54:    DJNZ    G53
        POP     HL
        POP     DE
        POP     BC
        RET

; N5AT: HL (0-65535) in decimal, right-aligned in B places (1-5; leading
; zeros spaces) at column E, colour pair C. Clobbers all but IX, IY.
N5BUF   EQU     WORK+36         ; its five digits
N5AT:   LD      A,C
        OUT     (XPAND),A
        LD      A,B
        PUSH    AF              ; the places
        PUSH    DE              ; the column (E)
        LD      DE,N5BUF
        LD      BC,10000
        CALL    N5DIG
        LD      BC,1000
        CALL    N5DIG
        LD      BC,100
        CALL    N5DIG
        LD      BC,10
        CALL    N5DIG
        LD      A,L
        ADD     A,'0'
        LD      (DE),A
        LD      HL,N5BUF        ; leading zeros: spaces
        LD      B,4
N5Z:    LD      A,(HL)
        CP      '0'
        JR      NZ,N5Y
        LD      (HL),' '
        INC     HL
        DJNZ    N5Z
N5Y:    POP     DE              ; E: the column
        POP     AF              ; the places: the last of the five
        LD      B,A
        NEG
        ADD     A,5
        LD      HL,N5BUF
        ADD     A,L
        LD      L,A
        JR      NC,N5X
        INC     H
N5X:    LD      A,(HL)
        CALL    GLY5
        INC     HL
        INC     E
        DJNZ    N5X
        RET
; N5DIG: (DE) = '0' + HL / BC; HL = HL mod BC; DE on.
N5DIG:  LD      A,'0'-1
N5D1:   INC     A
        OR      A
        SBC     HL,BC
        JR      NC,N5D1
        ADD     HL,BC
        LD      (DE),A
        INC     DE
        RET
