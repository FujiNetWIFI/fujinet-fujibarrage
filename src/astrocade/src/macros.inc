; macros.inc -- the macros every module uses. Included before any code:
; zmac resolves forward labels and EQUs, never forward macros.

; PAGEHDR: every page's first bytes: the 55H sentinel, the BIOS's menu
; link, the name, the start (RSTENT: page 0 again, a cold start), so a
; console RESET with the page mapped comes home. Its entries follow (E_*).
PAGEHDR MACRO
        DB      55H
        DW      MENUST
        DW      RNAME
        DW      RSTENT
        ENDM

; NOENT: a page's three entries, none yet (each a RET).
NOENT   MACRO
        RET
        NOP
        NOP
        RET
        NOP
        NOP
        RET
        NOP
        NOP
        ENDM

; FAR: a routine in another page (FARCALL: the caller's page comes back).
FAR     MACRO   pg, adr
        CALL    FARCALL
        DB      pg
        DW      adr
        ENDM

; GOPAGE: entry ent (E_*) of page pg (PGCALL: the caller's page comes back).
GOPAGE  MACRO   pg, ent
        LD      A,pg
        LD      HL,ent
        CALL    PGCALL
        ENDM

; SND: queue a sound or jingle.
SND     MACRO   id
        LD      A,id
        CALL    SNDQUE
        ENDM

; TXT: the string at adr, line lin, column cl, colour pair pair.
TXT     MACRO   adr, lin, cl, pair
        LD      HL,adr
        LD      DE,(lin)*256+(cl)
        LD      C,pair
        CALL    TXTAT
        ENDM

; XP: an expander colour pair: fg on bg (colour indexes 0-3).
XP      MACRO   fg, bg
        LD      C,(fg)*4+(bg)
        ENDM
