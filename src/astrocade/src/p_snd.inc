; p_snd.inc -- the sound: three tones and the noise of the custom I/O chip
; (FujiBlast's engine, grown: tunes voiced by instrument, a master the
; noise may set and modulate, the falling shell's whistle). The data is
; tools/astro/mksnd_astro.py's snd.inc: its header says what each channel
; does on the field and under a tune.
;
; SNDTICK runs from the frame interrupt (VBSND in core.inc, this page
; selected raw, the interrupted page put back after): the queued sounds
; started, the tune and each channel stepped, then all eight registers
; written at once (OTIR to the block port). It runs between any two
; instructions of the program, so it keeps the stack shallow and touches
; only the sound's own bytes.
;
; A channel (SNDCH, 7 bytes each, A B C N): its list (0: off), the frames
; left of its step, its volume; then an effect's decay, the decay's count
; and its T (the noise's: the master it asks for); a tune's instrument
; (and a raw step's volume, high nibble), its jiffies into the note (k)
; and the note. A tune's list lies in MUSBEG-MUSEND: that is how a channel
; knows what it plays.

        RET                     ; E_ENTER
        NOP
        NOP
        RET                     ; E_STATE
        NOP
        NOP
        JP      SNDTICK         ; E_TICK

CHSZ    EQU     7
MUSPT   EQU     SNDSH           ; MUSFEED's scratch (SNDSH is built after it)
MUSCN   EQU     SNDSH+2
WHISTV  EQU     5               ; the whistle's volume

SNDTICK: LD     A,(FLAGS)       ; muted: all off
        AND     FL_MUTE
        JP      NZ,SNMUTE
        LD      HL,SNDQ         ; the queue: two
        LD      A,(HL)
        LD      (HL),0FFH
        CALL    SNSTART
        LD      HL,SNDQ2
        LD      A,(HL)
        LD      (HL),0FFH
        CALL    SNSTART
        CALL    MUSCTL          ; the tune started or stopped
        LD      A,(MUSBAR)      ; anything to play? (a tune, a channel, the
        CP      0FDH            ;  whistle)
        JR      C,SQT1A
        LD      HL,SNDCH+1      ; (each channel's list, high byte)
        LD      A,(HL)
        LD      DE,CHSZ
        ADD     HL,DE
        OR      (HL)
        ADD     HL,DE
        OR      (HL)
        ADD     HL,DE
        OR      (HL)
        LD      HL,WHIST
        OR      (HL)
        JR      NZ,SQT1A
        LD      HL,SNDSIL       ; no: silence, once
        LD      A,(HL)
        OR      A
        RET     NZ
        LD      (HL),1
        LD      HL,SNDSH
        LD      B,8
SQT1B:  LD      (HL),0
        INC     HL
        DJNZ    SQT1B
        JP      SNOUT
SQT1A:  XOR     A
        LD      (SNDSIL),A
        CALL    MUSFEED
        CALL    SNSTEP
        CALL    MUSADV
; the registers, 7 down to 0
        LD      IX,SNDCH+3*CHSZ ; N: its volume; an effect's step may set the
        CALL    CHOUT           ;  master, and modulate it
        LD      A,E
        RLCA
        RLCA
        RLCA
        RLCA
        AND     0F0H
        LD      (SNDSH),A       ; 7: the noise's volume
        LD      A,SNDMO
        LD      (SNDSH+7),A     ; 0: the master
        XOR     A
        LD      (SNDSH+2),A     ; (5: its bits below)
        LD      A,(IX+0)        ; (off, or a tune's drums: the master as it is)
        OR      (IX+1)
        JR      Z,SQT2
        CALL    ISMUS
        JR      C,SQT2
        LD      A,(IX+6)
        OR      A
        JR      Z,SQT2
        BIT     7,A
        JR      Z,SQT1C
        LD      HL,SNDSH+2      ; modulated
        SET     4,(HL)
SQT1C:  ADD     A,A             ; (the master: T * 2)
        LD      (SNDSH+7),A
SQT2:   LD      A,E             ; the noise mixed in when it sounds
        OR      A
        JR      Z,SQT3
        LD      HL,SNDSH+2
        SET     5,(HL)
SQT3:   LD      IX,SNDCH+2*CHSZ ; C: 5's volume, 3: its period
        CALL    CHOUT
        LD      A,(SNDSH+2)
        OR      E
        LD      (SNDSH+2),A
        LD      A,D
        LD      (SNDSH+4),A
        LD      IX,SNDCH+1*CHSZ ; B: 6's high nibble, 2: its period
        CALL    CHOUT
        LD      A,D
        LD      (SNDSH+5),A
        LD      A,E
        RLCA
        RLCA
        RLCA
        RLCA
        LD      (SNDSH+1),A
        LD      IX,SNDCH        ; A: 6's low nibble, 1: its period (the
        CALL    CHOUT           ;  whistle's, on the field, when A is free)
        LD      A,(IX+1)
        OR      (IX+0)
        JR      NZ,SQT4
        LD      A,(FLAGS)
        AND     FL_FIELD
        JR      Z,SQT4
        LD      A,(WHIST)
        OR      A
        JR      Z,SQT4
        LD      D,A
        LD      E,WHISTV
SQT4:   LD      A,D
        LD      (SNDSH+6),A
        LD      A,(SNDSH+1)
        OR      E
        LD      (SNDSH+1),A
        XOR     A               ; 4: no vibrato
        LD      (SNDSH+3),A
SNOUT:  LD      HL,SNDSH
        LD      BC,8*256+SNDBX  ; (B after OUTI's DEC: registers 7 .. 0)
        OTIR
        RET

; SNMUTE: every channel stopped, the tune off, the chip silent.
SNMUTE: LD      HL,SNDCH
        LD      B,4*CHSZ
SQM1:   LD      (HL),0
        INC     HL
        DJNZ    SQM1
        LD      HL,SNDSH
        LD      B,8
SQM2:   LD      (HL),0
        INC     HL
        DJNZ    SQM2
        LD      A,0FFH
        LD      (SNDQ),A
        LD      (SNDQ2),A
        LD      (MUSBAR),A
        JR      SNOUT

; ISMUS: carry if channel IX plays a tune's list (in MUSBEG-MUSEND).
; Clobbers A, HL. ISM1: list HL's; clobbers A only. (Nothing pushed: it
; runs at the frame interrupt's deepest, on top of a shot's calls.)
ISMUS:  LD      L,(IX+0)
        LD      H,(IX+1)
ISM1:   LD      A,L             ; HL < MUSBEG: no
        SUB     MUSBEG AND 0FFH
        LD      A,H
        SBC     A,MUSBEG/256
        JR      C,ISM2
        LD      A,L             ; HL < MUSEND: yes
        SUB     MUSEND AND 0FFH
        LD      A,H
        SBC     A,MUSEND/256
        RET
ISM2:   OR      A
        RET

; CHOUT: channel IX now: D its period, E its volume (off: 0). Clobbers A,
; BC, HL.
CHOUT:  LD      A,(IX+0)
        OR      (IX+1)
        LD      E,A
        RET     Z
        LD      A,(IX+0)        ; ISMUS inline (the stack's deepest)
        SUB     MUSBEG AND 0FFH
        LD      A,(IX+1)
        SBC     A,MUSBEG/256
        JR      C,CHO1
        LD      A,(IX+0)
        SUB     MUSEND AND 0FFH
        LD      A,(IX+1)
        SBC     A,MUSEND/256
        JR      C,MVOICE
CHO1:
        LD      D,(IX+6)        ; an effect's step
        LD      E,(IX+3)
        RET

; MVOICE: a tune's step voiced this jiffy by its instrument (tools/mus.py's,
; as tools/mkmus_intv.py voices them; the volumes POKEY's, the chip's
; steps being linear too). D the period, E the volume.
MVOICE: LD      A,(IX+4)
        AND     15
        LD      C,(IX+5)        ; C: k
        LD      B,(IX+6)        ; B: the note
        OR      A
        JR      NZ,MV1
        LD      D,B             ; raw: the note is its period
        LD      A,(IX+4)
        RRCA
        RRCA
        RRCA
        RRCA
        AND     15
        LD      E,A
        INC     B
        DEC     B
        RET     NZ
        LD      E,0             ; (0: a rest)
        RET
MV1:    DEC     A
        JR      NZ,MV2
        LD      E,10            ; bass: 10, then 6 from its third jiffy
        LD      A,C
        CP      2
        JP      C,MVP
        LD      E,6
        JR      MVP
MV2:    DEC     A
        JR      NZ,MV3
        LD      E,6             ; blip: 6, 3, then nothing
        LD      A,C
        CP      2
        JR      C,MVP
        LD      E,3
        CP      4
        JR      C,MVP
        LD      E,0
        JR      MVP
MV3:    DEC     A
        JR      NZ,MV4
        LD      E,6             ; tone: 6, then 4
        LD      A,C
        CP      2
        JR      C,MVP
        LD      E,4
        JR      MVP
MV4:    DEC     A
        JR      Z,MVARP         ; arp: 0, 3, 7
        DEC     A
        JR      NZ,MV6
        LD      HL,ARPMAJ       ; arpmaj: 0, 4, 7
        JR      MVA1
MVARP:  LD      HL,ARPMIN
MVA1:   LD      A,C             ; (k / 2) mod 3: the chord's note now
        SRL     A
MVA2:   CP      3
        JR      C,MVA3
        SUB     3
        JR      MVA2
MVA3:   ADD     A,L
        LD      L,A
        JR      NC,MVA4
        INC     H
MVA4:   LD      A,B
        ADD     A,(HL)
        LD      B,A
        LD      E,5
        JR      MVP
MV6:    DEC     A
        JR      NZ,MV7
        LD      A,C             ; warble: up a semitone every other jiffy
        RRCA
        AND     1
        ADD     A,B
        LD      B,A
        LD      E,5
        JR      MVP
MV7:    DEC     A
        JR      NZ,MV8
        LD      D,KICKT1        ; kick: two jiffies falling, then nothing
        LD      E,15
        LD      A,C
        OR      A
        RET     Z
        LD      D,KICKT2
        LD      E,13
        DEC     A
        RET     Z
        LD      E,0
        RET
MV8:    DEC     A
        JR      NZ,MV9
        LD      E,12            ; snare (the noise): 12, 6, 6
        LD      A,C
        OR      A
        RET     Z
        LD      E,6
        CP      3
        RET     C
        LD      E,0
        RET
MV9:    LD      E,6             ; hat: one jiffy
        LD      A,C
        OR      A
        RET     Z
        LD      E,0
        RET
MVP:    LD      A,B             ; D: note B's period
        SUB     PITCHLO
        JR      NC,MVP1
        XOR     A
MVP1:   LD      HL,PITCHT
        ADD     A,L
        LD      L,A
        JR      NC,MVP2
        INC     H
MVP2:   LD      D,(HL)
        RET
ARPMIN: DB      0,3,7
ARPMAJ: DB      0,4,7

; SNSTART: sound A (0FFH: none) on its channels, cutting what was there;
; a tune's id starts the tune.
SNSTART: CP     NSNDID
        RET     NC
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      DE,SNDTAB
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        CP      MUSID0
        JR      C,SQS0
        LD      (MUSREC),DE     ; a tune: from its first bar
        LD      A,0FDH
        LD      (MUSBAR),A
        RET
SQS0:   EX      DE,HL           ; HL: its entry: channel, list, ... 0FFH
SQS1:   LD      A,(HL)
        CP      0FFH
        RET     Z
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        PUSH    HL
        LD      L,A             ; the channel: SNDCH + 7 ch
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     L
        LD      L,A
        LD      H,0
        LD      BC,SNDCH
        ADD     HL,BC
        LD      (HL),E          ; its list
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0          ; its step: fetched next
        INC     HL
        LD      (HL),0          ; quiet till then
        POP     HL
        JR      SQS1

; SNSTEP: a frame of each channel: its next step fetched when one ends; an
; effect's volume decays, a tune's note counts its jiffies.
SNSTEP: LD      IX,SNDCH
        LD      B,4
SQP1:   LD      L,(IX+0)
        LD      H,(IX+1)
        LD      A,H
        OR      L
        JP      Z,SQP9          ; off
        CALL    ISM1
        JR      C,SQPM
        LD      A,(IX+2)
        OR      A
        JR      NZ,SQP3
        LD      A,(HL)          ; the next step: T, volume, frames, decay
        LD      (IX+6),A
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      A
        JR      NZ,SQP2
        LD      (IX+0),A        ; frames 0: the end
        LD      (IX+1),A
        LD      (IX+3),A
        JR      SQP9
SQP2:   LD      (IX+2),A
        LD      A,(HL)
        INC     HL
        LD      (IX+4),A
        LD      (IX+5),A
        LD      (IX+0),L
        LD      (IX+1),H
        LD      (IX+3),C
SQP3:   DEC     (IX+2)          ; the step's frame
        LD      A,(IX+4)        ; the decay
        OR      A
        JR      Z,SQP9
        DEC     (IX+5)
        JR      NZ,SQP9
        LD      (IX+5),A
        LD      A,(IX+3)
        CP      2
        JR      C,SQP9
        DEC     (IX+3)
        JR      SQP9
SQPM:   LD      A,(IX+2)        ; a tune's: its next step (note, instrument,
        OR      A               ;  frames), or the next jiffy of this one
        JR      NZ,SQPM2
        LD      A,(HL)
        LD      (IX+6),A
        INC     HL
        LD      A,(HL)
        LD      (IX+4),A
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      A
        JR      NZ,SQPM1
        LD      (IX+0),A        ; the list's end: idle (MUSFEED: the next bar)
        LD      (IX+1),A
        LD      (IX+3),A
        JR      SQP9
SQPM1:  LD      (IX+2),A
        LD      (IX+0),L
        LD      (IX+1),H
        LD      (IX+5),0
        JR      SQPM3
SQPM2:  INC     (IX+5)
SQPM3:  DEC     (IX+2)
SQP9:   LD      DE,CHSZ
        ADD     IX,DE
        DEC     B
        JP      NZ,SQP1
        RET

; ---- the tune: bars (snd.inc's ST_*: its bars, its loop bar or 0FFH, then
; a bar's jiffies and the lists of A, B, C, N, 0 where it has none) --------------
; MUSBAR: the bar playing, or 0FDH start asked, 0FEH stop asked, 0FFH off;
; MUSJ the jiffy in the bar; MUSREC the tune. A channel that is idle (an
; effect has ended) or at a bar's start walks the bar's list to jiffy MUSJ,
; so the tune keeps time under the cues and comes back mid-bar.

; MUSREC: HL = the bar MUSBAR's record (the tune's, 9 bytes a bar).
MUSENT  MACRO
        LD      HL,(MUSREC)
        INC     HL
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      A,(MUSBAR)
        LD      L,A
        LD      H,0
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,BC
        ADD     HL,DE
        ENDM

; MUSCTL: the start and stop asked for.
MUSCTL: LD      A,(MUSBAR)
        CP      0FDH
        RET     C               ; going
        JR      Z,MZSTA
        CP      0FEH
        RET     NZ              ; off
        LD      IX,SNDCH        ; stop: the tune's lists off every channel
        LD      B,4
MZS1:   CALL    ISMUS
        JR      NC,MZS2
        XOR     A
        LD      (IX+0),A
        LD      (IX+1),A
        LD      (IX+2),A
        LD      (IX+3),A
MZS2:   LD      DE,CHSZ
        ADD     IX,DE
        DJNZ    MZS1
        LD      A,0FFH
        LD      (MUSBAR),A
        RET
MZSTA:  XOR     A               ; start: bar 0, jiffy 0
        LD      (MUSJ),A
        LD      (MUSBAR),A
        RET

; MUSFEED: before a jiffy's step, each channel the bar has a list for
; started where the bar is, if it is idle, or the bar has just begun and
; it was playing the tune.
MUSFEED: LD     A,(MUSBAR)
        CP      0FDH
        RET     NC
        MUSENT
        INC     HL              ; (the jiffies)
        LD      (MUSPT),HL
        LD      IX,SNDCH
        LD      A,4
        LD      (MUSCN),A
MZF1:   LD      HL,(MUSPT)      ; this channel's list for the bar
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (MUSPT),HL
        LD      A,D
        OR      E
        JR      Z,MZC9          ; (none: the cues')
        LD      A,(IX+0)
        OR      (IX+1)
        JR      Z,MZC2          ; idle: start it
        LD      A,(MUSJ)
        OR      A
        JR      NZ,MZC9         ; mid-bar, busy
        CALL    ISMUS           ; at the bar's start: only the tune's restarts
        JR      NC,MZC9
MZC2:   LD      A,(MUSJ)        ; walk the list DE to jiffy MUSJ
        LD      B,A
        EX      DE,HL
MZC3:   LD      A,(HL)          ; note
        LD      (IX+6),A
        INC     HL
        LD      A,(HL)          ; instrument
        LD      (IX+4),A
        INC     HL
        LD      A,(HL)          ; frames
        INC     HL
        OR      A
        JR      Z,MZC8          ; (past the end: idle)
        LD      D,A
        LD      A,B
        CP      D
        JR      C,MZC4          ; this step
        SUB     D
        LD      B,A
        JR      MZC3
MZC4:   LD      (IX+5),B        ; k: the jiffies into it
        LD      A,D
        SUB     B
        LD      (IX+2),A        ; the frames left of it
        LD      (IX+0),L
        LD      (IX+1),H
        LD      A,(IX+5)        ; (SNSTEP counts this jiffy: one back)
        OR      A
        JR      Z,MZC5
        DEC     (IX+5)
        JR      MZC9
MZC5:   DEC     HL              ; (a new step: SNSTEP fetches it, k 0)
        DEC     HL
        DEC     HL
        LD      (IX+0),L
        LD      (IX+1),H
        LD      (IX+2),0
        JR      MZC9
MZC8:   XOR     A
        LD      (IX+0),A
        LD      (IX+1),A
        LD      (IX+3),A
MZC9:   LD      DE,CHSZ
        ADD     IX,DE
        LD      HL,MUSCN
        DEC     (HL)
        JR      NZ,MZF1
        RET

; MUSADV: after a jiffy's step, the next jiffy of the bar, the next bar;
; past the last, the loop's bar, or the end (a jingle).
MUSADV: LD      A,(MUSBAR)
        CP      0FDH
        RET     NC
        MUSENT
        LD      A,(MUSJ)
        INC     A
        CP      (HL)
        JR      C,MZA1
        XOR     A
        LD      (MUSJ),A
        LD      HL,(MUSREC)     ; the bars, the loop
        LD      A,(MUSBAR)
        INC     A
        CP      (HL)
        JR      C,MZA2
        INC     HL
        LD      A,(HL)          ; (0FFH: the end; MUSCTL stops it)
        CP      0FFH
        JR      NZ,MZA2
        DEC     A
MZA2:   LD      (MUSBAR),A
        RET
MZA1:   LD      (MUSJ),A
        RET

        INCLUDE "snd.inc"
