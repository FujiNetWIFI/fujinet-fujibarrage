;; ===========================================================================
;; util.asm -- small helpers IntyBASIC is slow or clumsy at: a catalog byte
;; (two to a word), a 32-bit number in decimal.
;; ===========================================================================

;; CATB(k): USR: byte k of the catalog (CATALOG, two bytes a word, the
;; first in the low byte).
CATB:   PROC
        MOVR    R0, R1
        SLR     R1, 1
        ADDI    #const_CATALOG, R1
        MOVR    R1, R4
        MVI@    R4, R1
        ANDI    #1, R0
        BEQ     @@lo
        SWAP    R1
@@lo:   ANDI    #255, R1
        MOVR    R1, R0
        JR      R5
        ENDP

;; NUM32(lo, hi): the 32-bit number hi:lo in decimal at STXT (bytes, 0
;; ended, no leading zeros). USR: its length.
NUM32:  PROC
        PSHR    R5
        MVII    #const_STXT, R5         ; where the digits go
        MVII    #@@tens, R4
        CLRR    R3                      ; a digit put yet
        PSHR    R3                      ; [ret, any]
@@pow:  MVI@    R4, R2                  ; this power: R2 lo,
        PSHR    R4
        MVI@    R4, R3                  ; R3 hi
        PSHR    R5                      ; [ret, any, tens, out]
        CLRR    R5                      ; the digit
@@sub:  CMPR    R3, R1                  ; hi:lo >= power?
        BNC     @@done                  ; hi < power's
        BNEQ    @@take
        CMPR    R2, R0
        BNC     @@done
@@take: SUBR    R2, R0
        ADCR    R1                      ; (no borrow: carry set: hi += 1, then -1)
        DECR    R1
        SUBR    R3, R1
        INCR    R5
        B       @@sub
@@done: MOVR    R5, R2                  ; the digit
        PULR    R5                      ; out
        PULR    R4                      ; tens (at the power's hi)
        INCR    R4
        PULR    R3                      ; any
        TSTR    R2
        BNEQ    @@put
        TSTR    R3
        BNEQ    @@put
        CMPI    #@@last, R4             ; the ones: put even a 0
        BNEQ    @@skip
@@put:  ADDI    #48, R2
        MVO@    R2, R5
        MVII    #1, R3
@@skip: PSHR    R3
        CMPI    #@@last, R4
        BNEQ    @@pow
        PULR    R3
        CLRR    R0
        MVO@    R0, R5
        MOVR    R5, R0
        SUBI    #const_STXT + 1, R0
        PULR    PC
@@tens: DECLE   $CA00, $3B9A            ; 1000000000
        DECLE   $E100, $05F5            ; 100000000
        DECLE   $9680, $0098            ; 10000000
        DECLE   $4240, $000F            ; 1000000
        DECLE   $86A0, $0001            ; 100000
        DECLE   $2710, $0000            ; 10000
        DECLE   $03E8, $0000            ; 1000
        DECLE   $0064, $0000            ; 100
        DECLE   $000A, $0000            ; 10
        DECLE   $0001, $0000            ; 1
@@last:
        ENDP

;; PLB(s, f): USR: seat s's byte f in the state (P_*); PLW: its 16 bits
PLB:    PROC
        MOVR    R0, R2                  ; s * 25 = s * 16 + s * 8 + s
        SLL     R0, 2
        SLL     R0, 1
        ADDR    R0, R2
        SLL     R0, 1
        ADDR    R2, R0
        ADDR    R1, R0
        ADDI    #const_ST + const_OFF_SEATS, R0
        MOVR    R0, R4
        MVI@    R4, R0
        ANDI    #255, R0
        JR      R5
        ENDP
PLW:    PROC
        PSHR    R5
        CALL    PLB
        MVI@    R4, R1
        ANDI    #255, R1
        SWAP    R1
        ADDR    R1, R0
        PULR    PC
        ENDP
