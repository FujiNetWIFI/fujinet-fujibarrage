' ===========================================================================
' cat.bas -- the CATALOG (docs/PROTOCOL.md), as src/cat.c: the server's
' names for the arsenal, the host's settings and the bots' tiers. Kept as
' it came (CATALOG, two bytes a word: USR CATB(k) reads byte k) and indexed
' once it is whole (its length is in the STATE): CATIDX holds where each
' item, setting, page and tier starts.
' ===========================================================================

    CONST CI_ITEM = CATIDX              ' N_ITEMS
    CONST CI_SET  = CATIDX + 48         ' N_SETTINGS
    CONST CI_PAGE = CATIDX + 75         ' 4
    CONST CI_TIER = CATIDX + 79         ' N_TIERS

    DIM cat_ok
    DIM #ca                     ' where in the catalog
    DIM #cv_lo, #cv_hi, #cv_st, cv_dec
    UNSIGNED #cv_lo, #cv_st

' cat_check: index the catalog if it is whole (cat_ok = 1).
cat_check: PROCEDURE
    IF cat_ok THEN RETURN
    #v = rb(ST + OFF_CAT_LEN) + rb(ST + OFF_CAT_LEN + 1) * 256
    IF #v = 0 OR #v <> #cat_len THEN RETURN
    #ca = 4
    n = USR CATB(3)
    FOR i = 0 TO 3
        IF i < n THEN POKE CI_PAGE + i, #ca : #ca = #ca + 1 + USR CATB(#ca)
    NEXT i
    FOR i = 0 TO N_ITEMS - 1
        POKE CI_ITEM + i, #ca
        #ca = #ca + 6
        #ca = #ca + 1 + USR CATB(#ca)
    NEXT i
    FOR k = 0 TO N_SETTINGS - 1
        POKE CI_SET + k, #ca
        IF USR CATB(#ca + 1) = 0 THEN           ' a choice: page, 0, label, n, names
            #ca = #ca + 2
            #ca = #ca + 1 + USR CATB(#ca)
            n = USR CATB(#ca) : #ca = #ca + 1
            FOR i = 1 TO n
                #ca = #ca + 1 + USR CATB(#ca)
            NEXT i
        ELSE                                    ' a number: page, 1, label, min16, step16, count, decimals
            #ca = #ca + 2
            #ca = #ca + 1 + USR CATB(#ca) + 6
        END IF
    NEXT k
    n = USR CATB(#ca) : #ca = #ca + 1
    FOR i = 0 TO N_TIERS - 1
        IF i < n THEN POKE CI_TIER + i, #ca : #ca = #ca + 1 + USR CATB(#ca)
    NEXT i
    IF #ca <= #v THEN cat_ok = 1
END

' cat_str: the length-prefixed string at #ca into STXT (n at most), 0
' ended. n = its length.
cat_str: PROCEDURE
    IF cat_ok = 0 THEN POKE STXT, 0 : n = 0 : RETURN
    c = USR CATB(#ca)
    IF c < n THEN n = c
    FOR k = 1 TO n
        POKE STXT + k - 1, USR CATB(#ca + k)
    NEXT k
    POKE STXT + n, 0
END

' cat_item_name: item i's name into STXT (n at most).
cat_item_name: PROCEDURE
    #ca = PEEK(CI_ITEM + i) + 6
    GOSUB cat_str
END

' cat_tier_name: tier i's name into STXT (n at most).
cat_tier_name: PROCEDURE
    IF i >= N_TIERS THEN i = 0
    #ca = PEEK(CI_TIER + i)
    GOSUB cat_str
END

' cat_label: setting k's label into STXT (n at most).
cat_label: PROCEDURE
    #ca = PEEK(CI_SET + k) + 2
    GOSUB cat_str
END

' cat_count: v = how many values setting k has.
cat_count: PROCEDURE
    #ca = PEEK(CI_SET + k)
    t = USR CATB(#ca + 1)
    #ca = #ca + 2
    #ca = #ca + 1 + USR CATB(#ca)
    IF t = 0 THEN v = USR CATB(#ca) ELSE v = USR CATB(#ca + 4)
END

' cat_value: setting k at value v as the host's forms show it, into STXT;
' n = its length.
cat_value: PROCEDURE
    IF cat_ok = 0 THEN POKE STXT, 0 : n = 0 : RETURN
    #ca = PEEK(CI_SET + k)
    t = USR CATB(#ca + 1)
    #ca = #ca + 2
    #ca = #ca + 1 + USR CATB(#ca)
    IF t = 0 THEN
        c = USR CATB(#ca)
        IF v >= c THEN v = c - 1
        #ca = #ca + 1
        FOR j = 1 TO v
            #ca = #ca + 1 + USR CATB(#ca)
        NEXT j
        n = 16 : GOSUB cat_str
        RETURN
    END IF
    ' min + v steps (cash goes past 16 bits)
    #cv_lo = USR CATB(#ca) + USR CATB(#ca + 1) * 256
    #cv_hi = 0
    #cv_st = USR CATB(#ca + 2) + USR CATB(#ca + 3) * 256
    cv_dec = USR CATB(#ca + 5)
    FOR j = 1 TO v
        #w = #cv_lo + #cv_st
        IF #w < #cv_lo THEN #cv_hi = #cv_hi + 1
        #cv_lo = #w
    NEXT j
    n = USR NUM32(#cv_lo, #cv_hi)
    IF cv_dec THEN                      ' e.g. 20 with 2 decimals: 0.20
        WHILE n <= cv_dec               ' (8-bit counters: no FOR down to 0)
            j = n + 1
            WHILE j > 0
                POKE STXT + j, rb(STXT + j - 1)
                j = j - 1
            WEND
            POKE STXT, 48
            n = n + 1
        WEND
        j = n + 1
        WHILE j > n - cv_dec
            POKE STXT + j, rb(STXT + j - 1)
            j = j - 1
        WEND
        POKE STXT + n - cv_dec, 46
        n = n + 1
    END IF
END
