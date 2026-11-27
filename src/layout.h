/* layout.h -- where the text screens and the HUD put things: 40 columns
 * (the Atari, the Apple II, the CoCo 3) or 32 (the CoCo 1/2, the Adam: SCR_COLS,
 * scr.h). The 40-column values are the literals the screens always had. */
#ifndef LAYOUT_H
#define LAYOUT_H

#include "scr.h"

#ifdef __SMS__                          /* (the Master System's pad: buttons 1 and 2, Pause) */
#define TXT_PRESS "PRESS 1"
#define TXT_JOIN_HINT "UP/DOWN, 1 TO JOIN"
#define TXT_ITM_USE "<> 1 USE"
#define TXT_ITM_ON "<> 1 ON"
#define TXT_ITM_DONE "1 DONE"
#elif defined(__NES__)                  /* (the NES's pad: A, B, START, SELECT) */
#define TXT_PRESS "PRESS A"
#define TXT_JOIN_HINT "UP/DOWN, A TO JOIN"
#define TXT_ITM_USE "<> A USE"
#define TXT_ITM_ON "<> A ON"
#define TXT_ITM_DONE "A DONE"
#elif defined(__H89__)                  /* (no fire button: SPACE is it) */
#define TXT_PRESS "PRESS SPACE"
#define TXT_JOIN_HINT "UP/DOWN: CHOOSE   SPACE: JOIN   ESC: BACK"
#define TXT_ITM_USE "<> SPACE USE"
#define TXT_ITM_ON "<> SPACE ON"
#define TXT_ITM_DONE "SPACE DONE"
#else
#define TXT_PRESS "PRESS FIRE"
#define TXT_JOIN_HINT "UP/DOWN, FIRE TO JOIN"
#define TXT_ITM_USE "<> FIRE USE"
#define TXT_ITM_ON "<> FIRE ON"
#define TXT_ITM_DONE "FIRE DONE"
#endif

#if SCR_COLS == 32
/* the waiting room's and the results' seat rows */
#define SEAT_NAME_C  1
#define SEAT_KIND_C  10              /* BOT, (LINK DOWN) */
#define SEAT_TIER_C  14
#define SEAT_TIER_N  9
#define SEAT_HOST_C  24              /* HOST, WINS */
#define SEAT_WN_C    29
/* the settings' rows */
#define SET_NAME_C   1
#define SET_NAME_N   16
#define SET_VAL_C    18
/* the shop */
#define SHOP_NAME_N  13
#define SHOP_PRICE_C 15
#define SHOP_BUNDLE_C 21
#define SHOP_OWN_C   26
#define SHOP_ROUND_L "SHOP ROUND"
#define SHOP_ROUND_C 12
#define SHOP_CASH_L  15
#define SHOP_CASH_C  20
#define SHOP_TIMER_C 30
#define TXT_SHOP_HEAD "ITEM          PRICE BUY  OWN"
#ifdef __SMS__                  /* (the SMS: 1 fire, 2 TAB, 2+1 I/S, Pause RETURN) */
#define TXT_SHOP_HINT "1 BUY  LEFT SELL  PAUSE DONE"
#define TXT_W_HOSTSET "2+1 SEATS 2 PAGE <> SET 1 GO"
#define TXT_W_SET     "2+1 SEATS  2 PAGE"
#elif defined(__NES__)          /* (the NES: A fire, B TAB, B+A I/S, START RETURN) */
#define TXT_SHOP_HINT "A BUY  LEFT SELL  START DONE"
#define TXT_W_HOSTSET "B+A SEATS B PAGE <> SET A GO"
#define TXT_W_SET     "B+A SEATS  B PAGE"
#elif defined(__ADAM__)         /* (the Adam: RETURN, and a TAB key) */
#define TXT_SHOP_HINT "FIRE BUY  LEFT SELL  RETURN DONE"
#define TXT_W_HOSTSET "S SEATS TAB PAGE <> SET FIRE GO"
#define TXT_W_SET     "S SEATS  TAB PAGE"
#else
#define TXT_SHOP_HINT "FIRE BUY  LEFT SELL  ENTER DONE"
/* the waiting room's last row */
#define TXT_W_HOSTSET "S SEATS  W PAGE  <> SET FIRE GO"
#define TXT_W_SET     "S SEATS  W PAGE"
#endif
#ifdef __SMS__
#define TXT_W_WAIT    "WAITING FOR THE HOST  2+1: SET"
#define TXT_W_PEOPLE  "1 STARTS (2+ PEOPLE)  2+1: SET"
#define TXT_W_BOTS    "1 STARTS (BOTS FILL)  2+1: SET"
#elif defined(__NES__)
#define TXT_W_WAIT    "WAITING FOR THE HOST  B+A: SET"
#define TXT_W_PEOPLE  "A STARTS (2+ PEOPLE)  B+A: SET"
#define TXT_W_BOTS    "A STARTS (BOTS FILL)  B+A: SET"
#else
#define TXT_W_WAIT    "WAITING FOR THE HOST  S: SET"
#define TXT_W_PEOPLE  "FIRE STARTS (2+ PEOPLE) S: SET"
#define TXT_W_BOTS    "FIRE STARTS (BOTS FILL) S: SET"
#endif
/* the title */
#define TXT_TAGLINE  "ARTILLERY FOR TEN ON FUJINET"
#define TXT_GALORE   "WEAPONS, SHIELDS, BOTS GALORE."
/* the HUD: row 0 name, angle, power, energy; row 1 the weapon, the wind;
 * row 2 the round, what is happening, the timer */
#define HUD_ANG_L    9
#define HUD_ANG_C    13
#define HUD_POW_L    17
#define HUD_POW_C    21
#define HUD_E_L      26
#define HUD_E_C      28
#define TXT_ANGLE    "ANG"
#define TXT_POWER    "POW"
#define HUD_WPN_N    12
#define HUD_X_C      13
#define HUD_WIND_C   18
#define HUD_DIR_C    23
#define HUD_WV_C     24
#define HUD_TIMER_C  29
#define TXT_ROUND    "RD"
#define HUD_RD_C     3
#define TXT_OF       "/"
#define HUD_OF_C     5
#define HUD_RDS_C    6
#define HUD_MSG_C    9
#define TXT_AIM_FIRE "YOUR TURN: FIRE!"   /* (18 to the timer) */
/* the items line */
#define ITM_NAME_N   12
#define ITM_X_C      13
#define ITM_ON_C     18
#define ITM_USE_C    21
#define ITM_ON_TGT_C 22
#define ITM_FUEL_C   15
#define ITM_DONE_C   22
#elif SCR_COLS == 80
/* the H19: 80 columns (the H89), wider rows and longer names */
#define SEAT_NAME_C  4
#define SEAT_KIND_C  16
#define SEAT_TIER_C  22
#define SEAT_TIER_N  12
#define SEAT_HOST_C  44
#define SEAT_WN_C    52
#define SET_NAME_C   4
#define SET_NAME_N   28
#define SET_VAL_C    36
#define SHOP_NAME_N  22
#define SHOP_PRICE_C 26
#define SHOP_BUNDLE_C 34
#define SHOP_OWN_C   42
#define SHOP_ROUND_L "SHOP   ROUND"
#define SHOP_ROUND_C 14
#define SHOP_CASH_L  40
#define SHOP_CASH_C  45
#define SHOP_TIMER_C 76
#define TXT_SHOP_HEAD "ITEM                     PRICE   BUNDLE  OWN"
#define TXT_SHOP_HINT "SPACE BUYS   LEFT SELLS   UP DOWN: CHOOSE   RETURN: DONE"
#define TXT_W_HOSTSET "S SEATS   TAB PAGE   UP DOWN: CHOOSE   LEFT RIGHT: CHANGE   SPACE: GO"
#define TXT_W_SET     "S SEATS   TAB PAGE"
#define TXT_W_WAIT    "WAITING FOR THE HOST   S: SETTINGS"
#define TXT_W_PEOPLE  "SPACE STARTS (2+ PEOPLE)   S: SETTINGS"
#define TXT_W_BOTS    "SPACE STARTS (BOTS FILL)   S: SETTINGS"
#define TXT_TAGLINE  "ARTILLERY FOR UP TO TEN, ON FUJINET"
#define TXT_GALORE   "WEAPONS, SHIELDS AND BOTS GALORE."
#define HUD_ANG_L    12
#define HUD_ANG_C    18
#define HUD_POW_L    25
#define HUD_POW_C    31
#define HUD_E_L      38
#define HUD_E_C      40
#define TXT_ANGLE    "ANGLE"
#define TXT_POWER    "POWER"
#define HUD_WPN_N    20
#define HUD_X_C      21
#define HUD_WIND_C   28
#define HUD_DIR_C    33
#define HUD_WV_C     34
#define HUD_TIMER_C  76
#define TXT_ROUND    "ROUND"
#define HUD_RD_C     6
#define TXT_OF       "OF"
#define HUD_OF_C     9
#define HUD_RDS_C    12
#define HUD_MSG_C    18
#define TXT_AIM_FIRE "YOUR TURN: AIM, SPACE FIRES"
#define ITM_NAME_N   20
#define ITM_X_C      21
#define ITM_ON_C     27
#define ITM_USE_C    32
#define ITM_ON_TGT_C 32
#define ITM_FUEL_C   15
#define ITM_DONE_C   30
#define TXT_DEFENCES "SET UP DEFENCES, RETURN: DONE"
#define QUIT_HINT "ESC: QUIT"
#else
#define SEAT_NAME_C  2
#define SEAT_KIND_C  12
#define SEAT_TIER_C  16
#define SEAT_TIER_N  12
#define SEAT_HOST_C  30
#define SEAT_WN_C    35
#define SET_NAME_C   2
#define SET_NAME_N   20
#define SET_VAL_C    26
#define SHOP_NAME_N  18
#define SHOP_PRICE_C 21
#define SHOP_BUNDLE_C 27
#define SHOP_OWN_C   34
#define SHOP_ROUND_L "SHOP   ROUND"
#define SHOP_ROUND_C 14
#define SHOP_CASH_L  24
#define SHOP_CASH_C  29
#define SHOP_TIMER_C 36
#define TXT_SHOP_HEAD "ITEM                PRICE  BUNDLE  OWN"
#ifdef _COCO_BASIC_                     /* (the CoCo 3: no TAB key, W is one) */
#define TXT_W_HOSTSET "S SEATS  W PAGE  <> CHANGE  FIRE GO"
#define TXT_W_SET     "S SEATS  W PAGE"
#elif defined(__ATARI5200__)            /* (the keypad's 0, the lower button) */
#define TXT_W_HOSTSET "0 SEATS  LOWER PAGE  <> CHANGE  FIRE GO"
#define TXT_W_SET     "0 SEATS  LOWER BUTTON PAGE"
#else
#define TXT_W_HOSTSET "S SEATS  TAB PAGE  <> CHANGE  FIRE GO"
#define TXT_W_SET     "S SEATS  TAB PAGE"
#endif
#ifdef __ATARI5200__
#define TXT_W_WAIT    "WAITING FOR THE HOST  0 SETTINGS"
#define TXT_W_PEOPLE  "FIRE STARTS (2+ PEOPLE)  0 SETTINGS"
#define TXT_W_BOTS    "FIRE STARTS (BOTS FILL)  0 SETTINGS"
#else
#define TXT_W_WAIT    "WAITING FOR THE HOST  S SETTINGS"
#define TXT_W_PEOPLE  "FIRE STARTS (2+ PEOPLE)  S SETTINGS"
#define TXT_W_BOTS    "FIRE STARTS (BOTS FILL)  S SETTINGS"
#endif
#define TXT_TAGLINE  "ARTILLERY FOR UP TO TEN, ON FUJINET"
#define TXT_GALORE   "WEAPONS, SHIELDS AND BOTS GALORE."
#define HUD_ANG_L    10
#define HUD_ANG_C    16
#define HUD_POW_L    21
#define HUD_POW_C    27
#define HUD_E_L      33
#define HUD_E_C      35
#define TXT_ANGLE    "ANGLE"
#define TXT_POWER    "POWER"
#define HUD_WPN_N    16
#define HUD_X_C      17
#define HUD_WIND_C   24
#define HUD_DIR_C    29
#define HUD_WV_C     31
#define HUD_TIMER_C  37
#define TXT_ROUND    "ROUND"
#define HUD_RD_C     6
#define TXT_OF       "OF"
#define HUD_OF_C     9
#define HUD_RDS_C    12
#define HUD_MSG_C    16
#define TXT_AIM_FIRE "YOUR TURN: AIM, FIRE"
#define ITM_NAME_N   16
#define ITM_X_C      19
#define ITM_ON_C     24
#define ITM_USE_C    29
#define ITM_ON_TGT_C 29
#define ITM_FUEL_C   15
#define ITM_DONE_C   27
#if defined(_COCO_BASIC_) || defined(__MSDOS__)  /* (the CoCo 3, the PC: the key is ENTER) */
#define TXT_SHOP_HINT "FIRE BUYS  LEFT SELLS  ENTER: DONE"
#elif defined(__ATARI5200__)
#define TXT_SHOP_HINT "FIRE BUYS  LEFT SELLS  START: DONE"
#else
#define TXT_SHOP_HINT "FIRE BUYS  LEFT SELLS  RETURN: DONE"
#endif
#endif

#if defined(_COCO_BASIC_) || defined(__MSDOS__)
#define TXT_DEFENCES "SET UP DEFENCES, ENTER: DONE"
#elif defined(__ADAM__)
#define TXT_DEFENCES "SET UP DEFENCES, RETURN DONE"   /* (28: the timer at 29) */
#elif defined(__SMS__)
#define TXT_DEFENCES "SET UP DEFENCES, PAUSE DONE"
#elif defined(__NES__)
#define TXT_DEFENCES "SET UP DEFENCES, START DONE"
#elif defined(__ATARI5200__)
#define TXT_DEFENCES "SET UP DEFENCES, START: DONE"
#endif

#ifdef __MSDOS__
#define QUIT_HINT "ESC: LOBBY"          /* the title: ESC goes back to the lobby (menus.c) */
#endif
#ifdef __NES__                  /* (no QUIT_HINT: the cartridge has no way back to CONFIG) */
#define TXT_WHEEL1 "UP/DOWN: LETTER  A: TAKE IT"
#define TXT_WHEEL2 "LEFT: BACK  START: DONE"
#endif
#ifdef __SMS__
#define QUIT_HINT "2+PAUSE: LOBBY"      /* (2 held and Pause is ESC: src/sms/input.c) */
/* the name wheel's hints (menus.c) */
#define TXT_WHEEL1 "UP/DOWN: LETTER  1: TAKE IT"
#define TXT_WHEEL2 "LEFT: BACK  PAUSE: DONE"
#endif

#endif
