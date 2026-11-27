; ---------------------------------------------------------------------------
; sound.inc -- the tones (port 5 bits 7-6: $40 1 kHz, $80 500 Hz, $C0 a
; 120 Hz buzz; no volume). A tone rings and decays in about 9 ms and is
; struck only when the bits change; so a sound is a list of steps (tone,
; jiffies, strike: 0 once, 1 every jiffy, 2 every other), from
; tools/chf/mksnd_chf.py (SNDTAB, SNDPRI, in the DATA page: RAM); tone $FF
; and an address goes on there (the title's loop). SNDB is the tone now
; out: the engine sends it with every row (no INS 5), and every port-5 write
; here keeps the row's bits (INS 5).
;
; A new sound replaces the one playing (an effect cuts a jingle). The tick
; runs once a pass by DT, between the drawings.
; ---------------------------------------------------------------------------

; the sounds' ids are generated (build/channelf/snd.inc: SNDTAB too)

; SNDOUT -- tone r8 out: silence first if it is struck again (the bits must
;   change), then the tone; SNDB = it.  Leaf.  Clobbers A, r7, DC0.
SNDOUT: INS 5
        NI 03FH
        LR 7,A
        OUTS 5                  ; (silence: the next tone is struck)
        LR A,8
        STB SNDB
        AS 7
        OUTS 5
        POP

; SNDINIT -- nothing playing, the tone off.  Leaf.  Clobbers A, DC0.
SNDINIT: CLR
        STB SNDB
        STB SNDID+1
        LI 0FFH
        STB SNDID
        INS 5
        NI 03FH
        OUTS 5
        POP

; SNDPLAY -- sound r0 (none while muted; none over a sound of a higher
;   priority, SNDPRI: it cuts one of its own or lower).  Leaf.  Clobbers A,
;   r0, r7, r8, Q, DC0.
SNDPLAY: LDB SNDMUTE
        NI 0FFH
        BNZ SNP9
        LDB SNDID               ; one playing: its priority over ours?
        CI 0FFH
        BZ SNP1
        DCI SNDPRI
        ADC
        LM
        LR 7,A
        DCI SNDPRI
        LR A,0
        ADC
        LM
        COM                     ; playing + ~ours carries if playing > ours
        AS 7
        BC SNP9
SNP1:   LR A,0
        STB SNDID
        SL 1
        DCI SNDTAB
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR A,QU                 ; its first step
        STB SNDPTR
        LR A,QL
        STB SNDPTR+1
        CLR
        STB SNDT                ; (the tick takes the step at once)
SNP9:   POP

; SNDTICK -- a pass of the sound: DT jiffies off the step; at its end the
;   next step struck; the step's tone struck again as its rule says.
SNDTICK: LDB SNDID              ; (nothing playing: out before ENTER)
        CI 0FFH
        BNZ SNT0
        POP
SNT0:   ENTER
        LDB DT
        STB SNDN
SNT1:   LDB SNDT                ; the step over?
        NI 0FFH
        BNZ SNT3
        DCI SNDPTR              ; the next step: tone, jiffies, strike
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LR 8,A                  ; tone
        CI 0FFH                 ; $FF: on at the address after it (a loop)
        BNZ SNT2
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LR 8,A
SNT2:   LM
        LR 6,A                  ; jiffies
        LM
        LR 5,A                  ; strike
        LR Q,DC                 ; (the next step: kept before any STB moves DC0)
        LR A,5
        STB SNDMODE
        LR A,6
        NI 0FFH
        BZ SNTEND               ; (0 jiffies: the end)
        STB SNDT
        LR A,QU
        STB SNDPTR
        LR A,QL
        STB SNDPTR+1
        LR A,8
        STB SNDTONE
        PI SNDOUT
        CLR
        STB SNDAGE
        BR SNT4
SNT3:   AI -1                   ; a jiffy of it
        STB SNDT
        INCB SNDAGE
        LDB SNDMODE             ; struck again?
        NI 0FFH
        BZ SNT4
        CI 1
        BZ SNT3A
        LDB SNDAGE
        NI 1
        BNZ SNT4
SNT3A:  LDB SNDTONE
        NI 0FFH
        BZ SNT4
        LR 8,A
        PI SNDOUT
SNT4:   DECB SNDN
        BZ SNTX
        JMP SNT1
SNTX:   LEAVE
SNTEND: LI 0FFH                 ; over: silence
        STB SNDID
        CLR
        LR 8,A
        PI SNDOUT
        LEAVE

; SNDTMUT -- sound off, or on again (TIME).  Leaf.  Clobbers A, DC0.
SNDTMUT: LDB SNDMUTE
        XI 1
        STB SNDMUTE
        LI 0FFH
        STB SNDID
        CLR
        STB SNDB
        INS 5
        NI 03FH
        OUTS 5
        POP
