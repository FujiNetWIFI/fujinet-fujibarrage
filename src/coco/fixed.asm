*
* fixed.asm -- what lives at the same address in every CoCo program (the
* game and the menu, both machines): the clock, so that it keeps running
* across a chain (src/coco/chain.asm), and the test hook, so that a rig
* (tools/cocosim) drives the menu and the game alike. Section fixed, a bss
* at $0EF0 (src/coco/barrage{,3}.link): the top of the direct page
* ($0E00-$0EFF, src/6809/dp.inc), never in a file, so a chain's load
* leaves it be.
*
        section fixed,bss

        export  _frame_count,_vbi_ticks,_coco_swap,_coco_asked
        export  _inp_hook,_inp_hook_dir,_inp_hook_btn,_inp_hook_key

_frame_count    rmb     2       ; jiffies since boot
_vbi_ticks      rmb     1       ; the script's clock
_inp_hook       rmb     1       ; the test hook (src/coco/input.c)
_inp_hook_dir   rmb     1
_inp_hook_btn   rmb     1
_inp_hook_key   rmb     1
_coco_swap      rmb     1       ; the colours: the artifact phase swapped / composite
                rmb     6       ; ($0EF8: src/coco/chain.asm's way back to ROM mode)
_coco_asked     rmb     2       ; $C33C once the question has been answered (power-on: not)
