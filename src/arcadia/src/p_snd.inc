; p_snd.inc -- the sound's page: its notes (snddat.inc, tools/arc/mksnd_arc.py:
; from SNDBASE; the vblank reads them, fixa.inc VBN*; the title's are in a
; page of their own, PG_SNDT) and its offline test scenes (fixed B's TSCRN;
; tools/arcsim/scenes/snd*.txt record them, tools/arc/sndwav.py cuts them):
;   11  every effect and cue from id TESTHK+3 on, one every 3 seconds
;   12  tune TESTHK+3 (MUS_*), then quiet

        BCTA,UN SZTEST          ; E_TSND

; SZTEST: a blank text screen, then the scene's sounds; frames for ever.
SZTEST: ZBSR    *ZTEXT
        LODA,R0 TESTHK+3
        STRA,R0 T0              ; the id
        LODA,R0 TESTHK+2
        COMI,R0 12
        BCTR,EQ SZT5
SZT1:   LODA,R0 T0
        ZBSR    *ZSNDQ
        LODI,R0 180
        STRA,R0 T1
SZT2:   ZBSR    *ZFRAME
        LODA,R0 T1
        SUBI,R0 1
        STRA,R0 T1
        BCFR,EQ SZT2
        LODA,R0 T0
        ADDI,R0 1
        STRA,R0 T0
        COMI,R0 MUS_TITLE
        BCTR,LT SZT1
SZT4:   ZBSR    *ZFRAME
        BCTR,UN SZT4
SZT5:   LODA,R0 T0
        ZBSR    *ZSNDQ
        BCTR,UN SZT4

        IF      $ > SNDBASE
        ERROR   "the sound page's code runs into its notes"
        ENDIF
        ORG     SNDBASE
        INCLUDE "snddat.inc"
