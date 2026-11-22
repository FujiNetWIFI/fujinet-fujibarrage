;
; pages.s -- HGR page 1, the field, in main RAM ($2000-$3FFF): zeros in
; the file, which the loader skips (tools/a2loader).
;
        .segment "HGRPAGE"
        .res    $2000
