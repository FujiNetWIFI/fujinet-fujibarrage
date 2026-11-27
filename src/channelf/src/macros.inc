; ---------------------------------------------------------------------------
; macros.inc -- the F8 idioms this client is written in (FujiBlast's
; src/channelf macros.inc, after M.U.L.E.'s clients/channelf).
;
; Calls. PI keeps exactly one return address (PC1), and PI and JMP both load
; A with the target's high byte, so nothing travels in A across a call. A
; routine that calls anything starts with ENTER and returns with LEAVE (the
; BIOS K-stack from r16 (KSTK0), 21 deep; both clobber r7). A leaf returns with POP.
; LEAVE carries A back through r10, so a routine may return a value in A.
;
; Compares, unsigned, which the F8 has no instruction for:
;   CI n   sets C when n >= A, Z when n == A          (it computes n - A)
;   CM     sets C when (DC0) >= A, Z when equal       ((DC0) - A; DC0++)
;   LR A,x / COM / AS y   sets C when y > x           (y + ~x carries)
; BM and BP test bit 7 (signed): never use them for an unsigned bound.
; ---------------------------------------------------------------------------

ENTER   MACRO
        LR K,P
        PI PUSHK
        ENDM

LEAVE   MACRO
        LR 10,A
        PI POPK
        LR A,10
        PK
        ENDM

; Leave by jumping to a routine that returns to our caller (a tail call):
; one K-stack level fewer, and no LEAVE afterwards. Clobbers A, r7.
TAIL    MACRO target
        PI POPK
        LR P,K
        JMP target
        ENDM

; --- memory ----------------------------------------------------------------
LDB     MACRO addr              ; A = (addr); DC0 = addr+1
        DCI addr
        LM
        ENDM

STB     MACRO addr              ; (addr) = A; DC0 = addr+1
        DCI addr
        ST
        ENDM

; (addr) = n
SETB    MACRO addr, n
        LI n
        DCI addr
        ST
        ENDM

; r = (addr)
LDR     MACRO r, addr
        DCI addr
        LM
        LR r,A
        ENDM

; (addr) = r
STR     MACRO r, addr
        LR A,r
        DCI addr
        ST
        ENDM

; rlo, rhi = the 16-bit (addr), little-endian
LDW     MACRO rlo, rhi, addr
        DCI addr
        LM
        LR rlo,A
        LM
        LR rhi,A
        ENDM

STW     MACRO rlo, rhi, addr
        DCI addr
        LR A,rlo
        ST
        LR A,rhi
        ST
        ENDM

INCB    MACRO addr              ; (addr)++; A = the new value
        DCI addr
        LM
        INC
        DCI addr
        ST
        ENDM

DECB    MACRO addr
        DCI addr
        LM
        AI 0FFH
        DCI addr
        ST
        ENDM

; --- branches --------------------------------------------------------------
; unsigned A against a constant n (1..255)
BLTI    MACRO n, lab            ; A < n
        CI (n)-1
        BC lab
        ENDM

BGEI    MACRO n, lab            ; A >= n
        CI (n)-1
        BNC lab
        ENDM

BEQI    MACRO n, lab
        CI n
        BZ lab
        ENDM

BNEI    MACRO n, lab
        CI n
        BNZ lab
        ENDM

; Long branches: the condition inverted around a JMP. JMP loads A, so these
; are for places where A is dead.
JZ      MACRO lab
        BNZ m_skip
        JMP lab
m_skip:
        ENDM

JNZ     MACRO lab
        BZ m_skip
        JMP lab
m_skip:
        ENDM

JC      MACRO lab
        BNC m_skip
        JMP lab
m_skip:
        ENDM

JNC     MACRO lab
        BC m_skip
        JMP lab
m_skip:
        ENDM

; A = 0 sets Z (LR and LM do not touch the flags)
TSTA    MACRO
        NI 0FFH
        ENDM

; DC0 = DC0 + A, for any A 0..255 (ADC alone is signed, -128..127)
ADCU    MACRO
        NI 0FFH
        BP m_pos
        AI 080H                 ; A - 128: 0..127
        ADC
        LI 07FH
        ADC
        LIS 1
        ADC                     ; + 128 in all
        BR m_done
m_pos:  ADC
m_done:
        ENDM

; SND -- sound n queued (sound.inc SNDPLAY).  Clobbers A, r0.
SND     MACRO n
        LI n
        LR 0,A
        PI SNDPLAY
        ENDM

; NOPS n -- n NOPs (4 clocks each): settle padding.
NOPS    MACRO n
        IF (n)>0
        REPT n
        NOP
        ENDM
        ENDIF
        ENDM

; ADC3 a, b -- A = ra + rb + C, the carry out right (a 24-bit add's middle
; and top bytes). LNK alone can carry ($FF + 1) and the AS after it would
; then report none: that case is taken out of line (A = rb, C still set).
;   LR A,0 / AS 3 / LR 0,A / ADC3 1,4 / LR 1,A / ADC3 2,5 / LR 2,A
; adds r3-r5 to r0-r2.
ADC3    MACRO ra, rb
        LR A,ra
        LNK
        BC m_ov
        AS rb
        BR m_done
m_ov:   LR A,rb
m_done:
        ENDM

; ADCM rb -- A = (DC0) + rb + C (DC0 moves on), the carry out right: ADC3
; with the first byte from memory (LM keeps the flags).
ADCM    MACRO rb
        LM
        LNK
        BC m_ov
        AS rb
        BR m_done
m_ov:   LR A,rb
m_done:
        ENDM
