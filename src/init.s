.ifndef INIT_INC
INIT_INC =1
.include "global.s"
.include "codeWindow.s"
.include "debug.s"
.include "irqAsm.s"
.include "graphicsAsm.s"
.include "x16.inc"

; Import required C variables
.import _logicEntryAddressesLow
.import _logicEntryAddressesHigh
.import _bInitMemoryMangerInit

.segment "BANKRAM06"
.global _b6ClearInterpreterVariables
_b6ClearInterpreterVariables:
    ldx #0
@clear:
    stz GOLDEN_RAM + VARS_AREA_START_GOLDEN_OFFSET,x
    stz GOLDEN_RAM + FLAGS_AREA_START_GOLDEN_OFFSET,x
    inx
    bne @clear
    rts

_b6InitInterpreter:
    stz ZP_TMP
    stz ZP_TMP + 1
    stz ZP_PTR_LF 
    stz ZP_PTR_LF + 1
    stz ZP_PTR_LE  
    stz ZP_PTR_LE + 1
    stz ZP_PTR_B1
    stz ZP_PTR_B1 + 1
    stz ZP_PTR_B2 
    stz ZP_PTR_B2 + 1
    stz ZP_PTR_DISP
    stz ZP_PTR_DISP + 1
rts

.segment "CODE"

initFile: .byte "agi.cx16.init"
_loadInitBankAndInitMemory:
lda #$1
ldx #$8
ldy #$2

jsr SETLFS

lda #(_loadInitBankAndInitMemory - initFile)
ldx #<initFile
ldy #>initFile
jsr SETNAM

lda #$0
ldx #<USER_SPACE
ldy #>USER_SPACE
jsr LOAD

jsr _bInitMemoryMangerInit

rts
.endif
