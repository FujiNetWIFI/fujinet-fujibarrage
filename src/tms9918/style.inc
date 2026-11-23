; style.inc -- st_line's record: a field line's styles (field.asm's styles
; builds them; render.asm reads a cell's 8 lines with IY)
        defc    L_SKYC  = 0     ; all sky: its colour pair
        defc    L_SKYP  = 1     ; its pattern (0, or the stars' seed)
        defc    L_DIRTC = 2     ; all dirt: its colour pair
        defc    L_DIRTP = 3     ; its pattern (0)
        defc    L_MIXC  = 4     ; sky and dirt: dirt on sky
        defc    L_SIZE  = 5
