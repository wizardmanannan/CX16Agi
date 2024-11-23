#ifndef _CELTOVERAZP_H_
#define _CELTOVERAZP_H_

#include "zeroPointer.h"

#define  VERA_BYTES_PER_ROW  ZP_TMP_2
#define  BCOL  ZP_TMP_2 + 1
#define  VERA_ADDRESS  ZP_TMP_3
#define  VERA_ADDRESS_HIGH  ZP_TMP_4
#define CEL_ADDR  ZP_TMP_5
#define CEL_WIDTH  ZP_TMP_6
#define CEL_TO_VERA_GENERAL_TMP  ZP_TMP_7
#define CEL_FLIPPED  ZP_TMP_8
#define CEL_TO_VERA_IS_FORWARD_DIRECTION  ZP_TMP_8 + 1
#define CEL_HEIGHT  ZP_TMP_9
#define CEL_TRANS  ZP_TMP_9 + 1
#define COLOR  ZP_TMP_10
#define SPLIT_CEL_BANK  ZP_TMP_10 + 1
#define NO_SPLIT_SEGMENTS  ZP_TMP_12
#define SPLIT_COUNTER  ZP_TMP_12 + 1
#define SPLIT_CEL_SEGMENTS  ZP_TMP_13
//ZP_TMP_14 Vacant
#define X_VAL  ZP_TMP_18 + 1 //Ideally we would start from ZP_TMP_16 by TOTAL_ROWS cannot be moved for some reason TODO : Investigate
#define Y_VAL  ZP_TMP_19
#define P_NUM  ZP_TMP_19 + 1
#define CEL_BANK  ZP_TMP_20 + 1
#define NEXT_DATA_INDEX  ZP_TMP_21
#define BMP_BANK  ZP_TMP_21 + 1
#define BMP_DATA  ZP_TMP_22

#endif
