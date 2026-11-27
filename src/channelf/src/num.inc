; ---------------------------------------------------------------------------
; num.inc -- numbers as text (resident: every page and the player use them).
; ---------------------------------------------------------------------------

; DIG2S -- r1 (0-99) as one or two digits stored at DC0.  Leaf.
;   Clobbers A, r0, r1.
DIG2S:  CLR
        LR 0,A
NDG1:   LR A,1
        CI 9
        BC NDG2
        AI -10
        LR 1,A
        LR A,0
        INC
        LR 0,A
        BR NDG1
NDG2:   LR A,0
        NI 0FFH
        BZ NDG3
        AI '0'
        ST
NDG3:   LR A,1
        AI '0'
        ST
        POP

; DECB3 -- r0 (0-255) as 3 digits, right-aligned in spaces, at DC0.  Leaf.
;   Clobbers A, r0-r2.
DECB3:  CLR
        LR 1,A                  ; hundreds
        LR 2,A                  ; tens
DB31:   LR A,0
        CI 99
        BC DB32
        AI -100
        LR 0,A
        LR A,1
        INC
        LR 1,A
        BR DB31
DB32:   LR A,0
        CI 9
        BC DB33
        AI -10
        LR 0,A
        LR A,2
        INC
        LR 2,A
        BR DB32
DB33:   LR A,1
        NI 0FFH
        LI ' '
        BZ DB34
        LR A,1
        AI '0'
DB34:   ST
        LR A,1
        AS 2
        LI ' '
        BZ DB35
        LR A,2
        AI '0'
DB35:   ST
        LR A,0
        AI '0'
        ST
        POP

; DECW4 -- r1:r0 (0-9999) as 4 digits, right-aligned in spaces, at DC0.
;   Leaf.  Clobbers A, r0-r2, r6, r7.
DWDIG   MACRO d                 ; the digit of d: subtract while r1:r0 >= d
        CLR
        LR 7,A
m_l:    LR A,0
        AI 256-((d)&0FFH)
        LR 2,A
        LR A,1
        LNK
        AI 255-((d)>>8)
        BNC m_out
        LR 1,A
        LR A,2
        LR 0,A
        LR A,7
        INC
        LR 7,A
        BR m_l
m_out:  LR A,7                  ; a leading 0 is a space
        AS 6
        LI ' '
        BZ m_sp
        LIS 1
        LR 6,A
        LR A,7
        AI '0'
m_sp:   ST
        ENDM
DECW4:  CLR
        LR 6,A                  ; a digit written yet
        DWDIG 1000
        DWDIG 100
        DWDIG 10
        LR A,0
        AI '0'
        ST
        POP

; DEC32 -- the 32-bit N32 (LE) as 8 digits into NBUF, leading zeros as
;   spaces (the last digit always).  N32 is spent.  Leaf.
;   Clobbers A, r0-r11, DCs.  (Repeated subtraction of 10^7 .. 10^1.)
DEC32:  DCI N32                 ; r3:r2:r1:r0 = N32
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        DCI NBUF
        XDC                     ; DC1: the digits
        DCI PW10
DC32A:  LM                      ; ~10^k into r4-r7 (COM before the adds)
        COM
        LR 4,A
        LM
        COM
        LR 5,A
        LM
        COM
        LR 6,A
        LM
        COM
        LR 7,A
        LR Q,DC                 ; (the next power)
        CLR
        LR 11,A                 ; the digit
DC32B:  LI 0FFH                 ; x + ~p + 1: C when x >= p
        AI 1
        ADC3 0,4
        LR 8,A
        ADC3 1,5
        LR 9,A
        ADC3 2,6
        LR 10,A
        ADC3 3,7
        BNC DC32C
        LR 3,A                  ; x -= p, a digit more
        LR A,10
        LR 2,A
        LR A,9
        LR 1,A
        LR A,8
        LR 0,A
        LR A,11
        INC
        LR 11,A
        BR DC32B
DC32C:  LR A,11                 ; the digit, as a character
        AI '0'
        XDC
        ST
        XDC
        LR DC,Q
        LR A,QL                 ; past 10^1: the last digit is what is left
        CI (PW10END)&0FFH
        BNZ DC32A
        LR A,0
        AI '0'
        XDC
        ST
        DCI NBUF                ; leading zeros (not the last): spaces
        LIS 7
        LR 1,A
DC32D:  LM
        CI '0'
        BNZ DC32X
        LI 0FFH
        ADC
        LI ' '
        ST
        DS 1
        BNZ DC32D
DC32X:  POP
PW10:   DB 080H, 096H, 098H, 000H       ; 10 000 000
        DB 040H, 042H, 00FH, 000H       ; 1 000 000
        DB 0A0H, 086H, 001H, 000H       ; 100 000
        DB 010H, 027H, 000H, 000H       ; 10 000
        DB 0E8H, 003H, 000H, 000H       ; 1 000
        DB 064H, 000H, 000H, 000H       ; 100
        DB 00AH, 000H, 000H, 000H       ; 10
PW10END:
        IF (PW10>>8) <> (PW10END>>8)
        ERROR "PW10 must not cross a page (DC32C compares the low byte)"
        ENDIF

; NUMPUT -- NBUF's last r0 characters (1-8) at DC0.  Leaf.
;   Clobbers A, r0, DC1 (DC0 past them).
NUMPUT: XDC
        DCI NBUF+8
        LR A,0
        COM
        INC
        ADC                     ; NBUF + 8 - r0
        XDC
NPT1:    XDC
        LM
        XDC
        ST
        DS 0
        BNZ NPT1
        POP
