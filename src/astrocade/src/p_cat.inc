; p_cat.inc -- the CATALOG (src/cat.c), in ROM: the server's names for the
; arsenal, the host's settings and the bots' tiers, indexed at build time
; (tools/astro/mkcat_astro.py: cat.inc). Other pages read it through FAR
; calls: a name or a value is copied into WORK (NUL-ended).

        RET                     ; (no entries: FAR calls by name)
        NOP
        NOP
        RET
        NOP
        NOP
        RET
        NOP
        NOP

; CINAME: item A's name into WORK (A = its length). Clobbers BC, DE, HL.
CINAME: CALL    CIREC
        LD      DE,6
        ADD     HL,DE
        JP      CSTR

; CIINFO: item A: A its arms level, B its bundle, C its price's high byte,
; DE its low 16 bits. Clobbers HL.
CIINFO: CALL    CIREC
        INC     HL
        LD      A,(HL)          ; arms
        INC     HL
        LD      B,(HL)          ; bundle
        INC     HL
        LD      E,(HL)          ; price
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        RET
; CIREC: HL = item A's record (8 bytes). Clobbers DE.
CIREC:  LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,CITEMS
        ADD     HL,DE
        RET

; CSINFO: setting A: A its page, B its type (0 a choice), C its values'
; count. Clobbers DE, HL.
CSINFO: CALL    CSREC
        LD      A,(HL)
        INC     HL
        LD      B,(HL)
        INC     HL
        LD      C,(HL)
        RET
; CSREC: HL = setting A's record (8 bytes). Clobbers DE.
CSREC:  CP      CNSETS
        JR      C,CSR1
        XOR     A
CSR1:   LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,CSETS
        ADD     HL,DE
        RET

; CSLABEL: setting A's label into WORK. Clobbers BC, DE, HL.
CSLABEL: CALL   CSREC
        LD      DE,4
        ADD     HL,DE
        JP      CSTR

; CSVAL: setting A at value B into WORK, as the host's forms show it
; (cat.c cat_value): a choice's name, or min + B steps with its decimals.
; Clobbers all but IX, IY.
CSVAL:  CALL    CSREC
        INC     HL
        LD      A,(HL)          ; type
        INC     HL
        LD      C,(HL)          ; count
        INC     HL
        LD      E,(HL)          ; decimals
        LD      (WORK+38),A
        LD      A,E
        LD      (WORK+39),A
        INC     HL
        INC     HL
        INC     HL
        LD      E,(HL)          ; its data
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        LD      A,(WORK+38)
        OR      A
        JR      NZ,CSV3
        LD      A,B             ; a choice: its name (the last if past it)
        CP      C
        JR      C,CSV1
        LD      A,C
        DEC     A
CSV1:   ADD     A,A
        LD      E,A
        LD      D,0
        ADD     HL,DE
        JP      CSTR
CSV3:   LD      E,(HL)          ; a number: min + B steps (32 bits: cash)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (WORK+30),DE
        LD      DE,0
        LD      (WORK+32),DE
        LD      E,(HL)          ; the step
        INC     HL
        LD      D,(HL)
CSV4:   LD      A,B
        OR      A
        JR      Z,CSV5
        LD      HL,(WORK+30)
        ADD     HL,DE
        LD      (WORK+30),HL
        JR      NC,CSV4B
        LD      HL,(WORK+32)
        INC     HL
        LD      (WORK+32),HL
CSV4B:  DEC     B
        JR      CSV4
CSV5:   LD      HL,WORK+30
        LD      DE,WORK
        CALL    NUM32
        LD      A,(WORK+39)     ; decimals: e.g. 20 with 2 -> 0.20
        OR      A
        RET     Z
        LD      C,A
CSV6:   LD      HL,WORK         ; zeros in front until there's a digit before the point
        CALL    STRLEN
        CP      C
        JR      Z,CSV7
        JR      NC,CSV8
CSV7:   LD      E,A             ; one '0' in front
        LD      D,0
        LD      HL,WORK
        ADD     HL,DE           ; its NUL
        LD      B,A
        INC     B
CSV7A:  LD      A,(HL)
        INC     HL
        LD      (HL),A
        DEC     HL
        DEC     HL
        DJNZ    CSV7A
        LD      A,'0'
        LD      (WORK),A
        JR      CSV6
CSV8:   SUB     C               ; the point after len - dec digits
        LD      E,A
        LD      D,0
        LD      HL,WORK
        ADD     HL,DE           ; where it goes
        LD      B,C             ; move dec digits and the NUL one on
        INC     B
        PUSH    HL
        LD      E,C
        ADD     HL,DE           ; the NUL
CSV9:   LD      A,(HL)
        INC     HL
        LD      (HL),A
        DEC     HL
        DEC     HL
        DJNZ    CSV9
        POP     HL
        LD      (HL),'.'
        RET

; CPNAME: page A's name into WORK. CTNAME: tier A's. Clobber BC, DE, HL.
CPNAME: LD      HL,CPAGES
        CP      CNPAGES
        JR      C,CPT
        XOR     A
        JR      CPT
CTNAME: LD      HL,CTIERS
        CP      CNTIERS
        JR      C,CPT
        XOR     A
CPT:    ADD     A,A
        LD      E,A
        LD      D,0
        ADD     HL,DE
; CSTR: the string HL points at (a DW in a record: its address) into WORK.
; A = its length. Clobbers BC, DE, HL.
CSTR:   LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
CSTR2:  LD      DE,WORK
        LD      B,0
CST1:   LD      A,(HL)
        LD      (DE),A
        OR      A
        JR      Z,CST2
        INC     HL
        INC     DE
        INC     B
        LD      A,B
        CP      24
        JR      C,CST1
        XOR     A
        LD      (DE),A
CST2:   LD      A,B
        RET

; CCHECK: A = 1 if the state's catalog (OFF_CAT_LEN, OFF_CAT_SUM) is ours,
; else 0 (a server whose names and prices aren't these: UPDATE).
CCHECK: LD      HL,(ST+OFF_CAT_LEN)
        LD      DE,CATLEN
        OR      A
        SBC     HL,DE
        JR      NZ,CCK9
        LD      A,(ST+OFF_CAT_SUM)
        CP      CATS1
        JR      NZ,CCK9
        LD      A,(ST+OFF_CAT_SUM+1)
        CP      CATS2
        JR      NZ,CCK9
        LD      A,1
        RET
CCK9:   XOR     A
        RET

        INCLUDE "cat.inc"
