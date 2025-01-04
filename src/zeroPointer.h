#ifndef _ZEROPOINTER_H_
#define _ZEROPOINTER_H_

#define ZP_TMP_2 0xAD
#define ZP_TMP_3 0xAF
#define ZP_TMP_4 0xB1
#define ZP_TMP_5 0xB3
#define ZP_TMP_6 0xB5
#define ZP_TMP_7 0xB7
#define ZP_TMP_8 0xB9
#define ZP_TMP_9 0xBB
#define ZP_TMP_10 0xBD
#define ZP_TMP_12 0xBF
#define ZP_TMP_13 0xC1
#define ZP_TMP_14 0xC3
#define ZP_TMP_16 0xC5
#define ZP_TMP_17 0xC7
#define ZP_TMP_18 0xC9 // TODO: Where is CA - DA ? Why aren't we using them?
#define ZP_TMP_19 0xDB
#define ZP_TMP_20 0xDD
#define ZP_TMP_21 0xDF
#define ZP_TMP_22 0xE1
#define ZP_TMP_23 0xFE

#endif

extern byte SGC_LAST_LOCATION_GC_CHECKED;
#pragma zpsym("SGC_LAST_LOCATION_GC_CHECKED")


