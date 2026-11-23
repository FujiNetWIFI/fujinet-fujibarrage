*
* dp.inc -- the direct page, $0E00-$0EFF: the field engines' arguments and
* scratch (src/coco2/fld2.asm, src/coco3/fld3.asm) and the player's
* (src/6809/). C writes the arguments at these addresses (src/coco/fld.h);
* each C-callable entry sets DP to $0E and puts it back. $0EF0-$0EFF:
* src/coco/fixed.asm and src/coco/chain.asm's trampoline.
*
* The names are offsets in the page: lwasm (no SETDP in an object) takes a
* value under $100 as direct addressing, which DP = $0E puts on this page;
* a table's whole address is DPBASE + its name.
*
DPAGE   equ     $0E
DPBASE  equ     $0E00

* the engines' arguments (int16 big-endian, as the 6809 has them)
fx      equ     $00           ; x
fy      equ     $02           ; y
fx1     equ     $04           ; a span's last x / a run's end y (exclusive)
fv      equ     $06           ; the value, 0-3 (PX_*)
fold    equ     $07           ; fld_swap, fld_dots: the value to replace ($FF: any)
fent    equ     $08           ; fld_col: the TERRAIN entry; fld_dots: the shape
ex      equ     $0A           ; fld_ellipse: the centre
ey      equ     $0C
erx     equ     $0E           ;  the radii (ery at most 200)
ery     equ     $0F
emode   equ     $10           ;  0 all fv, 1 sky becomes fv, 2 fire becomes sky

* the engines' scratch
ft      equ     $18
ft2     equ     $19
val     equ     $1A
nm      equ     $1B
cur     equ     $1C
cnt     equ     $1D
xb      equ     $1E
don     equ     $1F
edy     equ     $20
ei      equ     $21
erem    equ     $22
ehw     equ     $23           ; 2
edw     equ     $25           ; 2: edy, 16 bits
dox     equ     $27           ; 2
doy     equ     $29           ; 2
dop     equ     $2B           ; 2
fp      equ     $2D           ; 2
fillt   equ     $30           ; 4: a value as a byte of it (the CoCo 1/2: the artifact phase)
invt    equ     $34           ; 4: a pixel's bits as its value
fswap   equ     $38           ; the artifact phase swapped (CoCo 1/2)
curblk  equ     $39           ; the screen block in slot 6 (CoCo 3; $FF: none known)
pm      equ     $3A           ; the row's pattern index mask (CoCo 3)
rawb    equ     $3B           ; a raw colour, both nibbles (CoCo 3: fld_dots)

* the player's: $0E40-$0EEF (src/6809/: ply, trk, ops, pt)
pp      equ     $40           ; 2: script + ply_pc
bi      equ     $42           ; a loop's count
bp      equ     $43           ; 2: the blast or settle in hand
fire    equ     $45           ; something burns this frame
den     equ     $46           ; scale: the divisor
mul     equ     $47           ; 2: scale's product
sn      equ     $49           ; (spare)
scnt    equ     $4A           ; scale: bits left
pfirst  equ     $4B           ; the shot's first frame (no step before its ops)
tslot   equ     $4C           ; trk: a loop's count
tfall   equ     $4D           ; the lowest falling shell's line | 1
tp      equ     $4E           ; 2: the track in hand
optr    equ     $50           ; 2: ops: the op's arguments
oi      equ     $52           ; a loop's count
ocnt    equ     $53
oto     equ     $54
omud    equ     $55
ofire   equ     $56
orec    equ     $57           ; 2: the path / flow / beam in hand
otmp    equ     $59
lx0     equ     $5A           ; 2: a beam's line
ly0     equ     $5C           ; 2
lx1     equ     $5E           ; 2
ly1     equ     $60           ; 2
ldx_    equ     $62           ; 2
ldy_    equ     $64           ; 2
lsx     equ     $66           ; 2
lsy     equ     $68           ; 2
lerr    equ     $6A           ; 2
le2     equ     $6C           ; 2
lon     equ     $6E
pcx     equ     $70           ; 2: sparks: where from
pcy     equ     $72
pleft   equ     $73
pkp     equ     $74
pd      equ     $75
pi      equ     $76
pseed   equ     $77
prec    equ     $78           ; 2: the spark in hand
plive   equ     $7A
pcnt    equ     $7B
pkind   equ     $7C

* the engines' fld_cols (a settle's columns, a byte's at a time): each
* column's TERRAIN entry read as runs (I_*: fld2.asm, fld3.asm)
its     equ     $D2           ; 24: four iterators (the CoCo 3 uses two)
gn      equ     $EA           ; columns left
gk      equ     $EB           ; the byte's columns (CoCo 1/2: 1-4)
gkeep   equ     $EC           ; the byte's bits that stay (CoCo 1/2)
I_P     equ     0             ; 2: an iterator: the entry's next end
I_K     equ     2             ;  ends left in the entry
I_V     equ     3             ;  the run's value (1 sky, 2 dirt)
I_E     equ     4             ;  the run's end row (the field's lines at most)
I_M     equ     5             ;  its column's bits in the byte (CoCo 1/2)
I_SIZE  equ     6

* the player calls C: DP 0 (BASIC's: the Disk ROM's code wants it) and slot 6
* the Disk ROM's around it; C keeps U and Y
CCALL   macro
        lbsr    fld_release
        clra
        tfr     a,dp
        jsr     \1
        lda     #DPAGE
        tfr     a,dp
        endm

* a C entry into the player: DP $0E, CMOC's U and Y kept, slot 6 given back
PENTRY  macro
        pshs    u,y,dp
        lda     #DPAGE
        tfr     a,dp
        lda     #$FF            ; (CoCo 3: no screen block known in slot 6)
        sta     <curblk
        lbsr    \1
        lbsr    fld_release
        puls    u,y,dp,pc
        endm
