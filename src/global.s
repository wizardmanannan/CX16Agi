.ifndef  GLOBAL_INC
GLOBAL_INC = 1

GOLDEN_RAM = $400

LOCAL_WORK_AREA_GOLDEN_OFFSET = 514
PARAMETERS_WORK_AREA_GOLDEN_OFFSET = 1015

LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET =  0
LOGIC_FILE_LOGIC_CODE_OFFSET =  2
LOGIC_FILE_LOGIC_BANK_OFFSET = 7

LOGIC_ENTRY_POINT_OFFSET = 1

.macro   GET_STRUCT_16 offset, pointer, result
         LDY   #offset
         LDA   (pointer),y
         STA   result
         LDY   #offset    + 1
         LDA   (pointer),y
         STA   result     + 1
.endmacro

.macro   SAVE_ZERO_PAGE firstPointer, saveLocation, noValues
         PHA
         TXA
         PHA

         LDX   #$0
         saveZPLoop:
         LDA   firstPointer,x
         STA   saveLocation,x
         inx
         cpx   #noValues
         bne   saveZPLoop

         PLA
         TAX
         PLA

.endmacro

.macro   RESTORE_ZERO_PAGE firstPointer, saveLocation, noValues
         LDX   #$0
         restoreZPLoop:
         LDA   saveLocation,x
         STA   firstPointer,x
         inx
         cpx   noValues
         bne   restoreZPLoop

.endmacro

.macro ADD_16_BYTE_ARRAY firstAddress, secondAddress
        clc


.endif
