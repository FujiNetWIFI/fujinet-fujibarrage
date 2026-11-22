; p_play.inc -- the field (src/play.c): entered (the round's sky and dirt,
; the tanks from the state, the HUD, the bands), a new state (the tanks
; that moved, our turn, the HUD), a frame (aiming: the stick's left and
; right the angle, up and down the power, + the next weapon, FIRE). And for
; the link: the whole field coming (a text screen blanked), its columns.
; The field's pixel primitives, the tanks, the HUD's font and the shimmer
; live here (other pages call them FAR).

        JP      PENTER          ; E_ENTER
        JP      PSTATE          ; E_STATE
        JP      PFRAME          ; E_TICK
        JP      PLOAD           ; E_LOAD: the whole field coming
        JP      PCOLSKY         ; E_COLSKY: column B all sky
        JP      PCOLRUN         ; E_COLRUN: column B, rows D to E-1, dirt
        JP      PSCRIPT         ; E_SCRIPT: a segment whole

; the field's own (GAMEV)
MYTURN  EQU     GAMEV           ; our turn: we aim
AIMA    EQU     GAMEV+1         ; our aim: the angle, the power (16), the weapon
AIMP    EQU     GAMEV+2
WEAPON  EQU     GAMEV+4
SENTA   EQU     GAMEV+5         ; the aim the server has (our last ACT_AIM)
SENTP   EQU     GAMEV+6
SENTT   EQU     GAMEV+8         ; the frame it went
HELDN   EQU     GAMEV+9         ; frames the stick has been held (the speed)
HUDDIRTY EQU    GAMEV+10        ; the HUD to draw again at the next frame
INSCR   EQU     GAMEV+11        ; a script under way (its segments)
RECOL   EQU     GAMEV+12        ; tanks moved: their colours at the next pass
TALKLEFT EQU    GAMEV+13        ; a talk over the HUD: its frames left
TALKOFF EQU     GAMEV+21        ;  its text in SCRIPT, its length, the letters
TALKN   EQU     GAMEV+22        ;  drawn
TALKI   EQU     GAMEV+23
IMENU    EQU     GAMEV+14        ; the items line: M_* (0 shut)
ITEM    EQU     GAMEV+15        ;  the item shown,
GUIDE   EQU     GAMEV+16        ;  the guidance for our next shot (I_NONE),
TARGET  EQU     GAMEV+17        ;  its target (NO_SEAT),
DRIVEN  EQU     GAMEV+18        ;  frames driving (an ACT_DRIVE each 8),
PRVD    EQU     GAMEV+19        ;  last frame's stick (a push is an edge)
HUDLAY  EQU     GAMEV+20        ; what the HUD's right half shows (M_*, M_DEF)
M_ITEM  EQU     1
M_TARGET EQU    2
M_DRIVE EQU     3
M_DEF   EQU     4               ; (HUDLAY: the defence window's prompt)

; ---- entered, a new state ----------------------------------------------------------
PENTER: XOR     A
        LD      (MYTURN),A
        LD      (INSCR),A
        LD      (IMENU),A
        LD      (TALKLEFT),A
        LD      (FLASHN),A
        LD      (SHAKE),A
        LD      A,0FFH          ; (I_NONE, NO_SEAT; the right half: redrawn)
        LD      (GUIDE),A
        LD      (TARGET),A
        LD      (HUDLAY),A
        XOR     A
        LD      HL,TKON         ; no tank drawn yet
        LD      B,SEATS
PE1:    LD      (HL),A
        INC     HL
        DJNZ    PE1
        LD      HL,SCR          ; the HUD's lines: clear
        LD      DE,SCR+1
        LD      BC,HUDH*BYTEPL-1
        LD      (HL),0
        LDIR
        LD      A,(NTV)         ; no field yet: all sky until it comes
        CP      0FFH
        JR      NZ,PE2
        LD      B,0
PE1A:   PUSH    BC
        CALL    PCOLSKY
        POP     BC
        INC     B
        LD      A,B
        CP      FIELD_W
        JR      C,PE1A
PE2:    LD      A,(ST+OFF_SKY)  ; the round's sky, dirt, fire
        AND     7
        CP      SKY_RANDOM
        JR      C,PE3
        LD      A,SKY_GRADIENT
PE3:    LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,SKYTAB
        ADD     HL,DE
        LD      (SKYTB),HL
        LD      A,(ST+OFF_PALETTE)
        AND     7
        LD      HL,DIRTTAB
        ADD     A,L
        LD      L,A
        JR      NC,PE4
        INC     H
PE4:    LD      A,(HL)
        LD      (DIRTC),A
        LD      A,FIRESTL
        LD      (FIREC),A
        LD      A,1             ; every tank from the state
        CALL    TKSTATE
        CALL    ISTURN
        LD      (MYTURN),A
        OR      A
        CALL    NZ,AIMINIT
        CALL    HUD
        FAR     PG_OPS,SOLVE
        JP      FIELDSCR

; PSTATE: a new state: the tanks that moved (all of them after terrain:
; its columns drew over them), our turn begun, the HUD.
PSTATE: LD      A,(INSCR)
        OR      A
        RET     NZ
        LD      A,(NTERR)
        PUSH    AF
        XOR     A
        LD      (NTERR),A
        POP     AF
        CALL    TKSTATE
        CALL    ISTURN
        LD      B,A
        LD      A,(MYTURN)
        CP      B
        LD      A,B
        LD      (MYTURN),A
        JR      Z,PS1
        OR      A
        JR      Z,PS1
        CALL    AIMINIT         ; our turn begins
        SND     UI_TURN
PS1:    LD      A,1
        LD      (HUDDIRTY),A
        RET

; ISTURN: A = 1 if we aim now: our seat, alive, the phase AIM, the turn
; ours (or everyone's and we're not done).
ISTURN: LD      A,(ST+OFF_PHASE)
        CP      PH_AIM
        JR      NZ,IST0
        LD      A,(MYSEAT)
        CP      SEATS
        JR      NC,IST0
        LD      B,A
        CALL    SEATP
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      C,(HL)
        LD      A,C
        AND     PF_ALIVE
        JR      Z,IST0
        LD      A,(ST+OFF_TURN)
        CP      B
        JR      Z,IST1
        CP      NO_SEAT
        JR      NZ,IST0
        LD      A,C
        AND     PF_DONE
        JR      NZ,IST0
IST1:   LD      A,1
        RET
IST0:   XOR     A
        RET

; AIMINIT: our aim from the state (the server's: where we left it).
AIMINIT: LD     A,(MYSEAT)
        CALL    SEATP
        PUSH    HL
        POP     IX
        LD      A,(IX+P_ANGLE)
        LD      (AIMA),A
        LD      (SENTA),A
        LD      L,(IX+P_POWER)
        LD      H,(IX+P_POWER+1)
        LD      (AIMP),HL
        LD      (SENTP),HL
        LD      A,(IX+P_WEAPON)
        LD      (WEAPON),A
        XOR     A
        LD      (HELDN),A
        RET

; TKSTATE: the tanks as the state has them (play.c tanks_from_state): A = 1
; all drawn again, else those that changed; their bands solved again if any
; moved (or all).
TKSTATE: LD     (T3),A          ; T3: all; T2: any moved
        XOR     A
        LD      (T2),A
        LD      C,0
TKS1:   PUSH    BC
        LD      A,C
        CALL    SEATP
        PUSH    HL
        POP     IX
        LD      A,(IX+P_KIND)   ; on: a seat, alive, on the field
        OR      A
        JR      Z,TKS2
        LD      A,(IX+P_FLAGS)
        AND     PF_ALIVE
        JR      Z,TKS2
        LD      A,(IX+P_X+1)
        OR      A
        JR      NZ,TKS2
        LD      A,(IX+P_X)
        CP      FIELD_W
        LD      A,1
        JR      C,TKS3
TKS2:   XOR     A
TKS3:   LD      D,A             ; D: on
        LD      B,0
        LD      HL,TKON
        ADD     HL,BC
        LD      A,(T3)          ; all, or changed?
        OR      A
        JR      NZ,TKS4
        LD      A,(HL)
        CP      D
        JR      NZ,TKS4
        OR      A
        JR      Z,TKS9          ; (off and off)
        LD      HL,TKX
        ADD     HL,BC
        LD      A,(IX+P_X)
        CP      (HL)
        JR      NZ,TKS4
        LD      HL,TKY
        ADD     HL,BC
        LD      A,(IX+P_Y)
        CP      (HL)
        JR      NZ,TKS4
        LD      HL,TKA
        ADD     HL,BC
        LD      A,(IX+P_ANGLE)
        CP      (HL)
        JR      Z,TKS9
        PUSH    DE              ; only its barrel: erased, drawn
        PUSH    BC
        LD      A,C
        CALL    TANKE
        POP     BC
        POP     DE
        JR      TKS6
TKS4:   LD      A,1             ; moved (or all)
        LD      (T2),A
        LD      HL,TKON
        ADD     HL,BC
        LD      A,(HL)
        OR      A
        JR      Z,TKS6
        PUSH    DE
        PUSH    BC
        LD      A,C
        CALL    TANKE
        POP     BC
        POP     DE
TKS6:   LD      HL,TKON         ; as the state has it
        ADD     HL,BC
        LD      (HL),D
        LD      HL,TKX
        ADD     HL,BC
        LD      A,(IX+P_X)
        LD      (HL),A
        LD      HL,TKY
        ADD     HL,BC
        LD      A,(IX+P_Y)
        LD      (HL),A
        LD      HL,TKA
        ADD     HL,BC
        LD      A,(IX+P_ANGLE)
        LD      (HL),A
        LD      A,D
        OR      A
        JR      Z,TKS9
        LD      A,C
        CALL    TANKD
TKS9:   POP     BC
        INC     C
        LD      A,C
        CP      SEATS
        JP      C,TKS1
        LD      A,(T2)
        OR      A
        RET     Z
        FAR     PG_OPS,SOLVE
        RET

; ---- the HUD -------------------------------------------------------------------------
; HUD: the turn's seat (ours if everyone aims): its name, angle, power,
; energy on its colour (left), the wind, the timer, its weapon and how many
; (right, the dark strip); the bars' lengths (power, energy).
HUD:    XOR     A
        LD      (HUDDIRTY),A
        LD      A,(ST+OFF_TURN)
        CP      SEATS
        JR      C,HU1
        LD      A,(MYSEAT)
        CP      SEATS
        JR      C,HU1
        XOR     A
HU1:    LD      (T0),A          ; T0: the seat shown
        LD      E,A             ; its colours
        LD      D,0
        LD      HL,SEATC
        ADD     HL,DE
        LD      A,(HL)
        LD      (HUDC),A
        LD      HL,SEATTX
        ADD     HL,DE
        LD      A,(HL)
        LD      (HUDC+1),A
        LD      A,HUDR0
        LD      (HUDC+2),A
        LD      A,HUDR1
        LD      (HUDC+3),A
        LD      A,(T0)          ; its name
        CALL    SNAME
        LD      HL,WORK
        LD      E,0
        LD      C,1*4+0
        LD      B,8
        CALL    TX5N
        LD      A,(T0)          ; its aim: ours as we have it
        CALL    SEATP
        PUSH    HL
        POP     IX
        LD      A,(IX+P_ANGLE)
        LD      L,(IX+P_POWER)
        LD      H,(IX+P_POWER+1)
        LD      C,(IX+P_WEAPON)
        LD      B,A
        LD      A,(MYTURN)      ; ours, aiming: our aim as we have it
        OR      A
        JR      Z,HU2
        LD      A,(MYSEAT)
        LD      D,A
        LD      A,(T0)
        CP      D
        JR      NZ,HU2
        LD      A,(AIMA)
        LD      B,A
        LD      HL,(AIMP)
        LD      A,(WEAPON)
        LD      C,A
HU2:    LD      A,C
        LD      (T1),A          ; T1: the weapon shown
        PUSH    HL
        LD      L,B             ; the angle (3), the power (4), the energy (3)
        LD      H,0
        LD      E,8
        LD      B,3
        LD      C,1*4+0
        CALL    N5AT
        POP     HL
        PUSH    HL
        LD      E,12
        LD      B,4
        LD      C,1*4+0
        CALL    N5AT
        LD      L,(IX+P_ENERGY)
        LD      H,0
        LD      E,17
        LD      B,3
        LD      C,1*4+0
        CALL    N5AT
        POP     HL              ; the bars: power / 25, energy * 2 / 5
        LD      DE,25
        CALL    DIVHL
        LD      A,L
        CP      41
        JR      C,HU3
        LD      A,40
HU3:    LD      (BARPW),A
        LD      A,(IX+P_ENERGY)
        ADD     A,A
        LD      L,A
        LD      H,0
        LD      DE,5
        CALL    DIVHL
        LD      A,L
        CP      41
        JR      C,HU4
        LD      A,40
HU4:    LD      (BARENG),A
        FAR     PG_OPS,SOBARS
        CALL    HUDR            ; the right half: the items line, the defence
        RET     NZ              ;  window's prompt, else the wind and the weapon
        LD      HL,(ST+OFF_WIND)        ; the wind: <n or >n
        LD      A,'>'
        BIT     7,H
        JR      Z,HU5
        LD      A,'<'
        EX      DE,HL
        LD      HL,0
        OR      A
        SBC     HL,DE
        LD      A,'<'
HU5:    PUSH    HL
        LD      E,21
        CALL    GLY5
        POP     HL
        LD      A,H
        OR      L
        JR      NZ,HU6
        LD      A,' '           ; (still: no arrow)
        LD      E,21
        CALL    GLY5
HU6:    LD      E,22
        LD      B,3
        LD      C,1*4+0
        CALL    N5AT
        LD      A,(ST+OFF_TIMER)        ; the timer
        LD      L,A
        LD      H,0
        LD      E,26
        LD      B,2
        LD      C,1*4+0
        CALL    N5AT
        LD      A,(T1)          ; the weapon (9 of its name), how many
        FAR     PG_CAT,CINAME
        LD      HL,WORK
        LD      E,29
        LD      C,1*4+0
        LD      B,9
        CALL    TX5N
        LD      A,(MYSEAT)      ; ours: how many (none shown for the mini
        LD      HL,T0           ;  missile: always), others': blank
        CP      (HL)
        LD      HL,SHU2
        LD      E,38
        LD      C,1*4+0
        LD      B,2
        JP      NZ,TX5N
        LD      A,(T1)
        CP      I_MINI_MISSILE
        JP      Z,TX5N
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_INV
        ADD     HL,DE
        LD      L,(HL)
        LD      H,0
HU7:    LD      E,38
        LD      B,2
        LD      C,1*4+0
        JP      N5AT

; HUDR: the HUD's right half (cells 21-39) cleared if what it shows changed;
; the items line or the defence prompt drawn (NZ), else Z: the usual.
HUDR:   LD      A,(IMENU)
        OR      A
        JR      NZ,HR1
        CALL    DEFEND
        LD      A,M_DEF
        JR      NZ,HR1
        XOR     A
HR1:    LD      HL,HUDLAY
        CP      (HL)
        JR      Z,HR3
        LD      (HL),A
        LD      HL,SCR+21       ; (a cell is a byte: 19 of them, 5 lines)
        LD      C,HUDH
HR2:    LD      B,19
        PUSH    HL
HR2A:   LD      (HL),0
        INC     HL
        DJNZ    HR2A
        POP     HL
        LD      DE,BYTEPL
        ADD     HL,DE
        DEC     C
        JR      NZ,HR2
HR3:    LD      A,(HUDLAY)
        OR      A
        RET     Z
        CP      M_ITEM
        JR      Z,HRITEM
        CP      M_TARGET
        JR      Z,HRTGT
        CP      M_DRIVE
        JR      Z,HRDRV
        LD      HL,SDEF         ; the defence window: x for the items, = done
        LD      E,21
        CALL    HRTXT
        LD      A,(ST+OFF_TIMER)
        LD      E,38
        JP      HRNUM
HRDRV:  LD      HL,SDRV
        LD      E,21
        CALL    HRTXT
        LD      A,(ST+OFF_INV+I_FUEL)
        LD      E,36
        JP      HRNUM
HRTGT:  LD      HL,STGT
        LD      E,21
        CALL    HRTXT
        LD      A,(TARGET)
        CP      SEATS
        JR      NC,HRT1
        CALL    SNAME
        LD      HL,WORK
        LD      E,28
        LD      C,1*4+0
        LD      B,8
        CALL    TX5N
HRT1:   OR      1
        RET
HRITEM: LD      A,(ITEM)        ; its name (11), x and how many, on or off
        FAR     PG_CAT,CINAME
        LD      HL,WORK
        LD      E,21
        LD      C,1*4+0
        LD      B,12            ; (the 12th: a space before the x)
        XOR     A
        LD      (WORK+11),A
        CALL    TX5N
        LD      A,'X'
        LD      E,33
        CALL    GLY5
        LD      A,(ITEM)
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_INV
        ADD     HL,DE
        LD      A,(HL)
        LD      E,34
        CALL    HRNUM
        LD      HL,SHU3         ; (neither: blank)
        LD      A,(ITEM)
        LD      B,PF_CHUTE
        CP      I_PARACHUTE
        JR      Z,HRI1
        LD      B,PF_FUSE
        CP      I_IMPACT_FUSE
        JR      NZ,HRI2
HRI1:   LD      A,(MYSEAT)
        CALL    SEATP
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      A,(HL)
        AND     B
        LD      HL,SONS
        JR      NZ,HRI2
        LD      HL,SOFFS
HRI2:   LD      E,37
        LD      C,1*4+0
        LD      B,3
        CALL    TX5N
        OR      1
        RET
HRTXT:  LD      C,1*4+0         ; text HL at cell E (15 cells)
        LD      B,15
        JP      TX5N
HRNUM:  LD      L,A             ; A at cell E (2 digits); NZ
        LD      H,0
        LD      B,2
        LD      C,1*4+0
        CALL    N5AT
        OR      1
        RET
SDEF:   DB      "X ITEMS = DONE",0
SDRV:   DB      "DRIVE <> FUEL",0
STGT:   DB      "TARGET",0
SONS:    DB      "ON",0
SOFFS:   DB      "OFF",0
SHU3:   DB      0

; DEFEND: NZ (A 1) in the defence window, ours and not done.
DEFEND: LD      A,(ST+OFF_PHASE)
        CP      PH_DEFENSE
        JR      NZ,DF0
        LD      A,(MYSEAT)
        CP      SEATS
        JR      NC,DF0
        CALL    SEATP
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      A,(HL)
        AND     PF_DEFENSE|PF_DONE
        CP      PF_DEFENSE
        JR      NZ,DF0
        LD      A,1
        OR      A
        RET
DF0:    XOR     A
        RET

; DIVHL: HL = HL / DE (16 bits, DE > 0). Clobbers A, BC, DE.
DIVHL:  LD      BC,0
DVH1:   OR      A
        SBC     HL,DE
        JR      C,DVH2
        INC     BC
        JR      DVH1
DVH2:   LD      H,B
        LD      L,C
        RET

; ---- a frame: aiming ------------------------------------------------------------------
; PFRAME: the HUD if a state asked; our turn: the stick aims (left and right
; the angle, faster the longer held; up and down the power), + the next
; weapon we have, FIRE fires; our aim to the server every 8 frames while it
; changes.
PFRAME: LD      A,(TALKLEFT)
        OR      A
        JR      NZ,PFR0
        LD      A,(HUDDIRTY)
        OR      A
        CALL    NZ,HUD
PFR0:   CALL    DEFEND
        LD      (T3),A          ; T3: defending
        LD      HL,MYTURN
        OR      (HL)
        JR      NZ,PFR1
        LD      A,(IMENU)        ; neither: the line shuts
        OR      A
        RET     Z
        FAR     PG_OPS,ICLOSE
        RET
PFR1:   LD      A,(T3)          ; the defence window: = is done
        OR      A
        JR      Z,PFR2
        LD      A,(EVKEY)
        CP      K_ENT
        JR      NZ,PFR2
        FAR     PG_OPS,ICLOSE
        LD      B,ACT_DONE
        LD      C,0
        CALL    ACTQ
        SND     UI_GO
        RET
PFR2:   LD      A,(IMENU)        ; the items line has the stick and FIRE
        OR      A
        JR      Z,PFR3
        FAR     PG_OPS,IFRAME
        RET
PFR3:   LD      A,(EVKEY)       ; x: it opens
        CP      K_MUL
        JR      NZ,PFR4
        FAR     PG_OPS,IOPEN
        RET
PFR4:
        LD      A,(T3)          ; (the defence window: items only)
        OR      A
        RET     NZ
        LD      A,(INDIR)       ; the stick: a step, more the longer it's held
        OR      A
        JR      NZ,PF1
        XOR     A
        LD      (HELDN),A
        JR      PF5
PF1:    LD      A,(HELDN)
        INC     A
        JR      Z,PF1A
        LD      (HELDN),A
PF1A:   LD      B,1             ; B: the step (1, 2 past 15 frames, 5 past 45)
        CP      16
        JR      C,PF2
        LD      B,2
        CP      46
        JR      C,PF2
        LD      B,5
PF2:    LD      A,(INDIR)
        AND     D_LEFT          ; left: the angle up (the barrel turns left)
        JR      Z,PF3
        LD      A,(AIMA)
        ADD     A,B
        CP      181
        JR      C,PF2A
        LD      A,180
PF2A:   LD      (AIMA),A
PF3:    LD      A,(INDIR)
        AND     D_RIGHT
        JR      Z,PF4
        LD      A,(AIMA)
        SUB     B
        JR      NC,PF3A
        XOR     A
PF3A:   LD      (AIMA),A
PF4:    LD      A,(INDIR)       ; up and down: the power, 5 a step
        AND     D_UP|D_DOWN
        JR      Z,PF4D
        LD      A,B
        ADD     A,A
        ADD     A,A
        ADD     A,B
        LD      E,A
        LD      D,0
        LD      HL,(AIMP)
        LD      A,(INDIR)
        AND     D_UP
        JR      Z,PF4B
        ADD     HL,DE
        LD      DE,1001
        PUSH    HL
        OR      A
        SBC     HL,DE
        POP     HL
        JR      C,PF4C
        LD      HL,1000
        JR      PF4C
PF4B:   OR      A
        SBC     HL,DE
        JR      NC,PF4C
        LD      HL,0
PF4C:   LD      (AIMP),HL
PF4D:   LD      A,1             ; (the HUD, and our tank's barrel)
        LD      (HUDDIRTY),A
        CALL    OURBARREL
PF5:    LD      A,(EVKEY)       ; +: the next weapon we have
        CP      K_PLUS
        CALL    Z,NEXTW
        LD      A,(EVBTN)       ; FIRE
        OR      A
        JP      NZ,PFIRE
        LD      HL,(FRAMES)     ; our aim to the server: every 8 frames, if new
        LD      A,(SENTT)
        LD      B,A
        LD      A,L
        SUB     B
        CP      8
        RET     C
        LD      A,(AIMA)
        LD      HL,SENTA
        CP      (HL)
        JR      NZ,PF6
        LD      HL,(AIMP)
        LD      DE,(SENTP)
        OR      A
        SBC     HL,DE
        RET     Z
PF6:    LD      A,(FRAMES)
        LD      (SENTT),A
        LD      A,(AIMA)
        LD      (SENTA),A
        LD      (AARGS),A
        LD      HL,(AIMP)
        LD      (SENTP),HL
        LD      (AARGS+1),HL
        LD      B,ACT_AIM
        LD      C,3
        JP      ACTQ
PFIRE:  LD      A,(AIMA)        ; angle, power16, weapon, guidance, target
        LD      (AARGS),A
        LD      HL,(AIMP)
        LD      (AARGS+1),HL
        LD      A,(WEAPON)
        LD      (AARGS+3),A
        LD      A,(GUIDE)       ; guidance and its target, if the items line
        LD      (AARGS+4),A     ;  chose them (then: none again)
        LD      A,(TARGET)
        LD      (AARGS+5),A
        LD      A,0FFH
        LD      (GUIDE),A
        LD      (TARGET),A
        LD      B,ACT_FIRE
        LD      C,6
        CALL    ACTQ
        XOR     A               ; fired: no more aiming
        LD      (MYTURN),A
        INC     A
        LD      (HUDDIRTY),A
        RET

; NEXTW: the next weapon we have (the mini missile always).
NEXTW:  LD      A,(WEAPON)
        LD      B,N_WEAPONS
NW1:    INC     A
        CP      N_WEAPONS
        JR      C,NW2
        XOR     A
NW2:    CP      I_MINI_MISSILE
        JR      Z,NW3
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_INV
        ADD     HL,DE
        LD      C,(HL)
        INC     C
        DEC     C
        JR      NZ,NW3
        DJNZ    NW1
        RET
NW3:    LD      (WEAPON),A
        LD      (AARGS),A       ; (the others see it: ACT_WEAPON)
        LD      A,1
        LD      (HUDDIRTY),A
        SND     UI_TICK
        LD      B,ACT_WEAPON
        LD      C,1
        JP      ACTQ

; OURBARREL: our tank drawn again at our aim's angle.
OURBARREL: LD   A,(MYSEAT)
        CP      SEATS
        RET     NC
        LD      E,A
        LD      D,0
        LD      HL,TKON
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        RET     Z
        LD      HL,TKA
        ADD     HL,DE
        LD      A,(AIMA)
        CP      (HL)
        RET     Z
        PUSH    HL
        PUSH    AF
        LD      A,E
        CALL    TANKE
        POP     AF
        POP     HL
        LD      (HL),A
        LD      A,(MYSEAT)
        JP      TANKD

; ---- the link's: the field coming, its columns, a segment ------------------------------
; PLOAD: the whole field coming: over a text screen, the screen blanked
; (the columns land in its lines); on the field, its tanks are drawn again
; after (PSTATE: NTERR).
PLOAD:  LD      A,(FLAGS)
        AND     FL_FIELD
        RET     NZ
        JP      TEXTBLANK

; PCOLSKY: column B all sky. PCOLRUN: column B, rows D to E-1, dirt.
PCOLSKY: LD     IX,PSKYE
        JP      FP_COLW
PCOLRUN: LD     A,E
        CP      FIELD_H+1
        JR      C,PCR1
        LD      E,FIELD_H
PCR1:   LD      A,E
        SUB     D
        RET     Z
        RET     C
        PUSH    AF
        PUSH    DE
        LD      C,0
        CALL    FP_ADR           ; HL = the column's top byte, A = its index
        LD      E,A
        LD      D,0
        PUSH    HL
        LD      HL,FP_MASK
        ADD     HL,DE
        LD      C,(HL)          ; the neighbours' mask
        LD      A,E
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      HL,FP_VAL+PX_DIRT
        ADD     HL,DE
        LD      A,(HL)
        LD      (FCDIRT),A
        POP     HL
        POP     DE
        CALL    FP_ROW           ; down to row D
        POP     AF
        LD      D,A
        LD      A,(FCDIRT)
        JP      FP_RUN
PSKYE:  DB      0F0H            ; an entry: no dirt
SHU2:   DB      0               ; (blank cells)

; PSCRIPT: (main.inc plays a segment itself: ML1A.)
PSCRIPT: RET

; ---- the script's ops about tanks, talk, damage, sound (play.c c_op) ------------
; COP: op A, its arguments at SCRIPT + PLYPC (it moves PLYPC past them).
COP:    LD      (T0),A
        CALL    RD8             ; the seat (OP_SOUND: the sound; OP_FLASH: the colour)
        LD      (T1),A
        CP      SEATS
        JR      C,CPO1
        LD      A,(T0)
        CP      OP_SOUND
        JR      Z,CPO1
        CP      OP_FLASH
        JR      Z,CPO1
        LD      A,NONE
        LD      (T1),A
CPO1:    LD      A,(T0)
        CP      OP_TALK
        JP      Z,CTALK
        CP      OP_SOUND
        JR      Z,CSOUND
        CP      OP_SHIELD
        JR      Z,CSHIELD
        CP      OP_DAMAGE
        JR      Z,CDAMAGE
        CP      OP_FALL
        JR      Z,CFALL
        CP      OP_DEATH
        JR      Z,CDEATH
        CP      OP_TANK
        JR      Z,CTANK
        CP      OP_FLASH
        JR      Z,CFLASH
        CP      OP_FIRE
        JP      Z,CFIRE
        RET
CSOUND: CALL    RD8             ; its size: a boom's id by it
        AND     3
        LD      C,A
        LD      A,(T1)
        CP      SND_BOOM
        JP      NZ,SNDQUE
        LD      A,SND_BOOM0
        ADD     A,C
        JP      SNDQUE
CSHIELD: CALL   RD8             ; event, hp
        JP      RD8
CDAMAGE: CALL   RD8             ; amount, energy left
        CALL    RD8
        LD      A,1
        LD      (HUDDIRTY),A
        RET
CFLASH: CALL    RD8             ; frames: the field white (VBISR)
        LD      (FLASHN),A
        RET
CFALL:  LD      A,(T1)          ; seat, x16, y, frames, chute
        CP      NONE
        JR      Z,CFL1
        LD      C,0
        CALL    TANKAT
        JR      CFL2
CFL1:   CALL    RD8
        CALL    RD8
        CALL    RD8
CFL2:   CALL    RD8
        JP      RD8
CTANK:  LD      A,(T1)          ; seat, x16, y, angle
        CP      NONE
        JR      Z,CTK1
        LD      C,1
        JP      TANKAT
CTK1:   CALL    RD8
        CALL    RD8
        CALL    RD8
        JP      RD8
CDEATH: CALL    RD8             ; seat, effect
        LD      A,(T1)
        CP      NONE
        RET     Z
        CALL    TANKE
        LD      A,(T1)
        LD      E,A
        LD      D,0
        LD      HL,TKON
        ADD     HL,DE
        LD      (HL),0
        LD      A,1
        LD      (RECOL),A
        RET
CFIRE:  CALL    RD8             ; seat, angle, power16, weapon
        LD      (T2),A
        LD      A,(T1)
        CP      NONE
        JR      Z,CFI1
        LD      E,A
        LD      D,0
        LD      HL,TKON
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      Z,CFI1
        LD      A,(T1)
        CALL    TANKE
        LD      A,(T1)
        LD      E,A
        LD      D,0
        LD      HL,TKA
        ADD     HL,DE
        LD      A,(T2)
        LD      (HL),A
        LD      A,(T1)
        CALL    TANKD
CFI1:   CALL    RD8
        CALL    RD8
        JP      RD8
; CTALK: seat, len, text: "NAME: text" over the HUD's row for 2 seconds.
CTALK:  CALL    RD8
        LD      (T2),A          ; its length
        LD      A,(T1)
        CP      NONE
        JR      Z,CTA9
        LD      E,A
        LD      D,0
        LD      HL,TKON
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      Z,CTA9
        LD      A,(T1)          ; the name
        CALL    SNAME
        LD      HL,WORK
        LD      E,0
        LD      C,1*4+0
        LD      B,8
        CALL    TX5N
        LD      A,':'
        LD      E,8
        CALL    GLY5
        LD      HL,SHU3         ; the rest of the row blank: the text comes a
        LD      E,9             ;  letter a frame, each with a blip (CANIM),
        LD      C,1*4+0         ;  31 at most, from SCRIPT (all of it at once
        LD      B,31            ;  if the segment ends first: TFLUSH)
        CALL    TX5N
        LD      A,(PLYPC)
        LD      (TALKOFF),A
        LD      A,(T2)
        CP      31
        JR      C,CTA2
        LD      A,31
CTA2:   LD      (TALKN),A
        XOR     A
        LD      (TALKI),A
        LD      A,120
        LD      (TALKLEFT),A
CTA9:   LD      A,(T2)          ; past the text
        LD      HL,PLYPC
        ADD     A,(HL)
        LD      (HL),A
        RET

; TANKAT: tank T1 moved (C 1: turned too): from the op, its x16, y (and
; angle); drawn again there (off the field: gone).
TANKAT: PUSH    BC
        LD      A,(T1)
        CALL    TANKE
        CALL    RD16
        PUSH    HL
        CALL    RD8
        LD      (T3),A
        POP     HL
        POP     BC
        LD      A,C
        OR      A
        JR      Z,TKT1
        PUSH    HL
        CALL    RD8
        POP     HL
        LD      C,A
        LD      A,(T1)
        LD      E,A
        LD      D,0
        PUSH    HL
        LD      HL,TKA
        ADD     HL,DE
        LD      (HL),C
        POP     HL
TKT1:   LD      A,(T1)
        LD      E,A
        LD      D,0
        LD      A,H             ; on the field?
        OR      A
        LD      A,0
        JR      NZ,TKT2
        LD      A,L
        CP      FIELD_W
        LD      A,0
        JR      NC,TKT2
        INC     A
TKT2:   PUSH    HL
        LD      HL,TKON
        ADD     HL,DE
        LD      (HL),A
        POP     HL
        LD      A,L
        LD      HL,TKX
        ADD     HL,DE
        LD      (HL),A
        LD      HL,TKY
        ADD     HL,DE
        LD      A,(T3)
        LD      (HL),A
        LD      A,1
        LD      (RECOL),A
        LD      HL,TKON
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        RET     Z
        LD      A,(T1)
        JP      TANKD

; RD8: A = SCRIPT[PLYPC++]. RD16: HL. Preserve BC, DE.
RD8:    PUSH    HL
        LD      A,(PLYPC)
        LD      HL,SCRIPT
        ADD     A,L
        LD      L,A
        JR      NC,RD81
        INC     H
RD81:   LD      A,(PLYPC)
        INC     A
        LD      (PLYPC),A
        LD      A,(HL)
        POP     HL
        RET
RD16:   CALL    RD8
        LD      L,A
        CALL    RD8
        LD      H,A
        RET

; CANIM: a step's talk: its time runs out, the HUD back.
CANIM:  LD      A,(TALKLEFT)
        OR      A
        RET     Z
        CALL    TLETTER         ; a letter, a blip
        JR      Z,CAN1
        CP      ' '
        JR      Z,CAN1
        SND     UI_TALK
CAN1:   LD      A,(TALKLEFT)
        DEC     A
        LD      (TALKLEFT),A
        RET     NZ
        JR      TALKEND         ; (its time over)
; TLETTER: the talk's next letter drawn (NZ, A it), or Z: all drawn.
TLETTER: LD     A,(TALKI)
        LD      HL,TALKN
        CP      (HL)
        RET     NC
        INC     A
        LD      (TALKI),A
        ADD     A,8             ; its cell: 9 on
        LD      E,A
        LD      A,(TALKI)
        DEC     A
        LD      HL,TALKOFF
        ADD     A,(HL)
        LD      HL,SCRIPT
        ADD     A,L
        LD      L,A
        JR      NC,TLT1
        INC     H
TLT1:   LD      A,1*4+0
        OUT     (XPAND),A
        LD      A,(HL)
        CALL    GLY5
        OR      1               ; (A kept: GLY5 preserves BC, DE, HL only)
        LD      A,(HL)
        RET
; TFLUSH: the segment's end: the talk's letters left, all now (its bytes go).
TFLUSH: LD      A,(TALKLEFT)
        OR      A
        RET     Z
TF1:    CALL    TLETTER
        JR      NZ,TF1
        RET

; TALKEND: a talk's row cleared (to the ground: its cells between the HUD's
; fields would stay), the HUD to be drawn again.
TALKEND: XOR    A
        LD      (TALKLEFT),A
        LD      HL,SCR
        LD      DE,SCR+1
        LD      BC,HUDH*BYTEPL-1
        LD      (HL),A
        LDIR
        INC     A
        LD      (HUDDIRTY),A
        RET

; PDRAWN: what a drawn pass owes: the tanks' colours if they moved (once a
; pass), the HUD if a state or an op asked (not over a talk).
PDRAWN: LD      A,(TALKLEFT)        ; (the tanks' colours: PLY's, first)
        OR      A
        RET     NZ
        LD      A,(HUDDIRTY)
        OR      A
        RET     Z
        JP      HUD

; PSCREND: the script's end: its talk gone, fire out, the states taken
; again (the tanks all drawn again at the next: settles cut through them).
PSCREND: LD     A,(TALKLEFT)    ; a talk cut short: its row cleared
        OR      A
        CALL    NZ,TALKEND
        XOR     A
        LD      (INSCR),A
        LD      A,1
        LD      (HUDDIRTY),A
        LD      (NTERR),A
        DI
        LD      A,(FLAGS)
        AND     0FFH-FL_FIRE
        LD      (FLAGS),A
        EI
        RET

; NOTE: the pixel primitives, the tanks, the HUD's font, the shimmer:
        INCLUDE "srcdir/src/fld.inc"
        INCLUDE "srcdir/src/tank.inc"
        INCLUDE "srcdir/src/hud.inc"
        INCLUDE "tables.inc"
        INCLUDE "font35.inc"
