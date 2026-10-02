; Check if global definitions are included, if not, include them
.ifndef  IRQ_INC
IRQ_INC = 1

.include "global.s"
.include "globalGraphics.s"
.include "spriteIrqHandler.s"
.include "inputIrqHandler.s"

.import _mainLoopCounter
.import _menuDirty
.import _inputLineDirty

.macro SEND_IRQ_COMMAND command, vSyncToCheck
sei
lda command
sta sendIrqCommand
lda _vSyncCounter
ldx _vSyncCounter + 1
sta vSyncToCheck
stx vSyncToCheck + 1
REENABLE_INTERRUPTS
.endmacro

.macro WAIT_FOR_NEXT_IRQ vSyncToCheck
.local @waitForIrq
.local @end

@waitForIrq:
sei
lda vSyncToCheck
ldx vSyncToCheck + 1
cmp _vSyncCounter
bne @end
cpx _vSyncCounter + 1
bne @end

; A masked WAI wakes on a pending IRQ; CLI lets the handler consume it.
; Checking first also catches a command completed before entering this wait.
wai
cli
bra @waitForIrq

@end:
cli
.endmacro

.macro WAIT_FOR_IRQ_COMPLETION completionToCheck
.local @waitForCompletion
.local @complete

@waitForCompletion:
sei
lda completionToCheck
cmp irqCompletionGeneration
bne @complete
lda completionToCheck + 1
cmp irqCompletionGeneration + 1
bne @complete
wai
cli
bra @waitForCompletion

@complete:
cli
.endmacro

;Handlers
.segment "BANKRAM03"
_b3PaletteAddress: .res 4 ;Should be three bytes, but there is no data type in C for handling 3 byte values
_b3PaletteRows: .byte $0
_b3PaletteNumber: .byte $0

.ifndef ZP_DISPLAY_TEXT_SOURCE_POINTER
ZP_DISPLAY_TEXT_SOURCE_POINTER = IRQ_TMP_6
.endif

DISPLAY_TEXT_SLICE_BYTES = 512
DISPLAY_PALETTE_SLICE_ROWS = 32

; These cursors survive the other IRQ work between bounded slices.
displayTextSourcePointer: .word $0
displayTextVeraAddress: .res 3
displayTextBytesRemaining: .word $0
displayTextSliceResult: .byte $0
displayPaletteCursor: .res 3
displayPaletteRowsRemaining: .byte $0
displayPaletteRowsThisSlice: .byte $0
displayPaletteValue: .byte $0

b3InitDisplayTextSlice:
lda _currentTextBuffer
sta displayTextSourcePointer
lda _currentTextBuffer + 1
sta displayTextSourcePointer + 1
lda _displayTextAddressToCopyTo
sta displayTextVeraAddress
lda _displayTextAddressToCopyTo + 1
sta displayTextVeraAddress + 1
lda _displayTextAddressToCopyTo + 2
and #$1
sta displayTextVeraAddress + 2
lda _b3PaletteAddress
sta displayPaletteCursor
lda _b3PaletteAddress + 1
sta displayPaletteCursor + 1
lda _b3PaletteAddress + 2
and #$1
sta displayPaletteCursor + 2
lda _b3PaletteRows
sta displayPaletteRowsRemaining
lda _b3PaletteNumber
asl
asl
asl
asl
sta displayPaletteValue
rts

b3SetPaletteSlice:
lda displayPaletteRowsRemaining
beq @paletteComplete
cmp #DISPLAY_PALETTE_SLICE_ROWS
bcc @paletteSliceCountReady
lda #DISPLAY_PALETTE_SLICE_ROWS
@paletteSliceCountReady:
sta displayPaletteRowsThisSlice

SET_VERA_ADDRESS_ABSOLUTE displayPaletteCursor, #$0, #$2

@paletteRow:
lda displayPaletteValue
ldx #TILE_LAYER_WIDTH
@paletteByte:
sta VERA_data0
dex
bne @paletteByte

; One tile-map row is 64 cells, or TILE_LAYER_WIDTH * 2 bytes.
clc
lda displayPaletteCursor
adc #TILE_LAYER_WIDTH * 2
sta displayPaletteCursor
lda displayPaletteCursor + 1
adc #$0
sta displayPaletteCursor + 1
lda displayPaletteCursor + 2
adc #$0
and #$1
sta displayPaletteCursor + 2
dec displayPaletteRowsRemaining
dec displayPaletteRowsThisSlice
bne @paletteRow

lda displayPaletteRowsRemaining
beq @paletteComplete
lda #$0
rts
@paletteComplete:
lda #$1
rts

_b3InitLayer1Mapbase:
SET_VERA_ADDRESS_IMMEDIATE (MAP_BASE + MENU_BAR_WIDTH * 2), #$0, #$1

ldx #< (TILE_LAYER_NO_TILES - MENU_BAR_WIDTH)
lda #TRANSPARENT_CHAR
ldy #TILE_BYTE_2

; The low-byte remainder is followed by exactly the full pages in the count.
cpx #$0
beq @fullPages
@remainder:
sta VERA_data0
sty VERA_data0
dex
bne @remainder

@fullPages:
lda #> (TILE_LAYER_NO_TILES - MENU_BAR_WIDTH)
beq @mapClearDone
sta ZP_TILE_LAYER_NO_TILES_HIGH
lda #TRANSPARENT_CHAR
ldy #TILE_BYTE_2
@page:
ldx #$0
@pageLoop:
sta VERA_data0
sty VERA_data0
dex
bne @pageLoop
dec ZP_TILE_LAYER_NO_TILES_HIGH
bne @page
@mapClearDone:
lda #$1
sta _inputLineDirty
rts

handleDisplayTextSlice:
SET_VERA_ADDRESS_ABSOLUTE displayTextVeraAddress, #$0, #$2

lda displayTextSourcePointer
sta ZP_DISPLAY_TEXT_SOURCE_POINTER
lda displayTextSourcePointer + 1
sta ZP_DISPLAY_TEXT_SOURCE_POINTER + 1
lda #<DISPLAY_TEXT_SLICE_BYTES
sta displayTextBytesRemaining
lda #>DISPLAY_TEXT_SLICE_BYTES
sta displayTextBytesRemaining + 1

@nextCharacter:
ldy #$0
lda (ZP_DISPLAY_TEXT_SOURCE_POINTER), y
beq @textComplete

cmp #NEW_LINE
bne @storeTextCharacter
jsr b3DisplayTextNewLine
bra @consumeTextCharacter ; Newline advances the VERA cursor without a tile write

@storeTextCharacter:
sta VERA_data0

@consumeTextCharacter:
inc ZP_DISPLAY_TEXT_SOURCE_POINTER
bne @sourcePointerReady
inc ZP_DISPLAY_TEXT_SOURCE_POINTER + 1
@sourcePointerReady:
lda displayTextBytesRemaining
bne @decrementTextBudget
dec displayTextBytesRemaining + 1
@decrementTextBudget:
dec displayTextBytesRemaining
lda displayTextBytesRemaining
ora displayTextBytesRemaining + 1
beq @textBudgetComplete
bra @nextCharacter

@textComplete:
lda #$1
bra @saveTextState

@textBudgetComplete:
lda #$0

@saveTextState:
sta displayTextSliceResult
lda VERA_addr_low
sta displayTextVeraAddress
lda VERA_addr_high
sta displayTextVeraAddress + 1
lda VERA_addr_bank
and #$1
sta displayTextVeraAddress + 2
lda ZP_DISPLAY_TEXT_SOURCE_POINTER
sta displayTextSourcePointer
lda ZP_DISPLAY_TEXT_SOURCE_POINTER + 1
sta displayTextSourcePointer + 1
lda displayTextSliceResult
rts

b3DisplayTextNewLine: ;Goes to the start of the next line of the VERA but maintains the column number of the first character of the first line
clc
lda #TILE_LAYER_WIDTH * 2
adc _displayTextAddressToCopyTo
sta _displayTextAddressToCopyTo
lda #$0
adc _displayTextAddressToCopyTo + 1
sta _displayTextAddressToCopyTo + 1
lda #$0
adc _displayTextAddressToCopyTo + 2
and #$1
sta _displayTextAddressToCopyTo + 2

SET_VERA_ADDRESS_ABSOLUTE _displayTextAddressToCopyTo, #$0, #$2
rts

_displayTextAddressToCopyTo: .word $0
_displayTextAddressToCopyToHigh: .word $0
_currentTextBuffer: .word $0
.segment "BANKRAM06"
_b6InitIrq:
 ; backup default RAM IRQ vector
   lda IRQVec
   sta default_irq_vector
   lda IRQVec+1
   sta default_irq_vector+1

   ; overwrite RAM IRQ vector with custom handler address
   sei ; disable IRQ while vector is changing
   lda #<custom_irq_handler
   sta IRQVec
   lda #>custom_irq_handler
   sta IRQVec+1
   lda #VSYNC_BIT ; make VERA only generate VSYNC IRQs
   sta VERA_ien
   REENABLE_INTERRUPTS ; enable IRQ now that vector is properly set
rts


ZP_TILE_LAYER_NO_TILES_HIGH = ZP_TMP_2

_b6SetAndWaitForIrqStateAsm:
sta @state
lda @state
beq @waitForVSync
sei
lda irqCompletionGeneration
sta @completionToCheck
lda irqCompletionGeneration + 1
sta @completionToCheck + 1
lda @state
sta sendIrqCommand
REENABLE_INTERRUPTS
WAIT_FOR_IRQ_COMPLETION @completionToCheck
rts

@waitForVSync:
SEND_IRQ_COMMAND @state, @vSyncToCheck
WAIT_FOR_NEXT_IRQ @vSyncToCheck
rts
@state: .byte $0
@vSyncToCheck: .word $0
@completionToCheck: .word $0

.segment "CODE"

IRQ_CMD_DONTCHANGE = 0
IRQ_CMD_BLACKSCREEN = 1
IRQ_CMD_TEXT_ONLY = 2
IRQ_CMD_NORMAL = 3
IRQ_CMD_DISPLAY_TEXT = 4
IRQ_CMD_L0_L1_ONLY = 5
IRQ_CMD_CLEAR = 6
IRQ_CMD_GRAPHICS = 7


LAYER_0_1_SPRITES_ENABLE = $71
LAYER_0_1_SPRITES_DISABLE = $1
LAYER_0_1_ENABLE_SPRITES_SPRITES_DISABLE = $19
LAYER_0_SPRITES_DISABLE_1_ENABLE = $21

;0 Don't Change
;1 Blank Screen
;2 Text Only
;3 Return To Normal Display (Both layers enabled)
;4 Display Text (A command to display text, the text to display is stored in the buffer pointed to by _currentTextBuffer)

sendIrqCommand: .byte $0

;As above except it will never change to 0
currentIrqState: .byte $0
lastMainLoopCounter: .byte $FF
_vSyncCounter: .word $0
_vSyncCounterHigh: .word $0
debugVSyncCounter: .word $0

.export _vSyncCounterHigh, _b6CopyVSyncCounter32

; __fastcall__ pointer arrives in A/X. Keep its publication and the copy
; inside the same interrupt-masked interval; IRQ_TMP_6 is private to IRQ work.
_b6CopyVSyncCounter32:
php
sei
sta IRQ_TMP_6
stx IRQ_TMP_6 + 1
ldy #$0
lda _vSyncCounter
sta (IRQ_TMP_6),y
iny
lda _vSyncCounter + 1
sta (IRQ_TMP_6),y
iny
lda _vSyncCounterHigh
sta (IRQ_TMP_6),y
iny
lda _vSyncCounterHigh + 1
sta (IRQ_TMP_6),y
plp
rts

IRQ_WORK_CLEAR = 1
IRQ_WORK_DISPLAY_TEXT = 2
IRQ_CLEAR_STAGE_MAP = 0
IRQ_CLEAR_STAGE_BACKGROUND = 1
IRQ_CLEAR_STAGE_PRIORITY = 2
IRQ_TEXT_STAGE_COPY = 0
IRQ_TEXT_STAGE_PALETTE = 1

irqCompletionGeneration: .word $0
irqWorkKind: .byte $0
irqClearStage: .byte IRQ_CLEAR_STAGE_MAP
irqTextStage: .byte IRQ_TEXT_STAGE_COPY
irqSavedDcVideo: .byte $0

custom_irq_handler:
lda RAM_BANK
sta @previousRamBank 

; continue to default IRQ handler
lda VERA_isr
and #VSYNC_BIT
bne @callSoundHandler
jmp @defaultIqr

@callSoundHandler:
; Apply display movement before the expensive handlers, during blanking.
; Self-modifying count in writable resident RAM (cacheless W65C02).
; Foreground STA and IRQ DEC publish one byte atomically; skip idle calls.
@shakeCountLoad:
lda #0
beq @handleSound
jsr handleScreenShake
@handleSound:
lda #SOUND_BANK
sta RAM_BANK
jsr bBSoundHandler

@handleDisplayInputLine:
lda #PARSER_BANK
sta RAM_BANK
jsr b7HandleInputLine

@handleMenuBar:
lda _menuDirty
beq @handleStatusBar

lda #MENU_BANK
sta RAM_BANK 

jsr bADisplayMenu

@handleStatusBar:
lda #STATUSBAR_BANK
sta RAM_BANK
jsr b10DisplayStatusBar

@checkMainLoopCounter:
lda _mainLoopCounter
cmp lastMainLoopCounter
beq @sendIrqCommand
sta lastMainLoopCounter

@handleSpriteUpdates:
lda #SPRITE_UPDATES_BANK
sta RAM_BANK
jsr bEHandleSpriteUpdates

@sendIrqCommand:
lda sendIrqCommand
tax

lda @jmpTableBank, x
sta RAM_BANK
txa
clc
asl
tax
jmp (@jmpTableIrq,x)


@displayText:
jsr b3InitDisplayTextSlice
lda #IRQ_TEXT_STAGE_COPY
sta irqTextStage
lda #IRQ_WORK_DISPLAY_TEXT
sta irqWorkKind
stz sendIrqCommand
jsr @servicePendingWork
jmp @vSyncCounter

@blankScreen:
stz VERA_ctrl
lda #LAYER_0_1_SPRITES_DISABLE
sta VERA_dc_video
lda #IRQ_CMD_BLACKSCREEN
sta currentIrqState
jmp @resetSetIrqState

@normal:
stz VERA_ctrl
lda #LAYER_0_1_SPRITES_ENABLE
sta VERA_dc_video
lda #IRQ_CMD_NORMAL
sta currentIrqState

lda sendIrqCommand
cmp #IRQ_CMD_NORMAL
bne @normalSetup
jmp @resetSetIrqState

@normalSetup:
lda #TEXT_BANK
sta RAM_BANK
jsr _b3InitLayer1Mapbase
lda #GRAPHICS_BANK
sta RAM_BANK
jsr _b6InitInput

jmp @resetSetIrqState

@textOnly:
stz VERA_ctrl
lda #LAYER_0_SPRITES_DISABLE_1_ENABLE
sta VERA_dc_video

lda #TEXT_BANK
sta RAM_BANK
jsr _b3InitLayer1Mapbase
jmp @resetSetIrqState

@l12Only:
stz VERA_ctrl
lda #LAYER_0_1_ENABLE_SPRITES_SPRITES_DISABLE
sta VERA_dc_video
lda #IRQ_CMD_L0_L1_ONLY
sta currentIrqState
jmp @resetSetIrqState

@clear:
stz VERA_ctrl
lda VERA_dc_video
sta irqSavedDcVideo
and #$8F
sta VERA_dc_video
lda #IRQ_CLEAR_STAGE_MAP
sta irqClearStage
lda #IRQ_WORK_CLEAR
sta irqWorkKind
stz sendIrqCommand
jsr @servicePendingWork
jmp @vSyncCounter

@showObj:
lda #SHOW_OBJ_BANK
sta RAM_BANK
jsr b11ShowObjIrqHandler
jmp @resetSetIrqState

@noCommand:
lda irqWorkKind
bne @noCommandHasWork
jmp @vSyncCounter
@noCommandHasWork:
jsr @servicePendingWork
jmp @vSyncCounter

@servicePendingWork:
lda irqWorkKind
cmp #IRQ_WORK_CLEAR
bne @serviceCheckText
jmp @serviceClear
@serviceCheckText:
cmp #IRQ_WORK_DISPLAY_TEXT
bne @serviceWorkUnknown
jmp @serviceDisplayText
@serviceWorkUnknown:
rts

@serviceClear:
lda irqClearStage
cmp #IRQ_CLEAR_STAGE_MAP
beq @serviceClearMap
cmp #IRQ_CLEAR_STAGE_BACKGROUND
beq @serviceClearBackground

lda #PICTURE_CODE_OVERFLOW_BANK
sta RAM_BANK
jsr _b8ClearPrioritySlice
beq @serviceWorkReturn
stz irqWorkKind
stz VERA_ctrl
lda irqSavedDcVideo
sta VERA_dc_video
inc irqCompletionGeneration
bne @serviceWorkReturn
inc irqCompletionGeneration + 1
rts

@serviceClearMap:
lda #TEXT_BANK
sta RAM_BANK
jsr _b3InitLayer1Mapbase
lda #SPRITE_UPDATES_BANK
sta RAM_BANK
jsr _bEClearSpriteAttributes
lda #GRAPHICS_BANK
sta RAM_BANK
jsr _b6InitInput
lda #PICTURE_CODE_OVERFLOW_BANK
sta RAM_BANK
jsr _b8ClearBackgroundSliceReset
jsr _b8ClearPrioritySliceReset
lda #IRQ_CLEAR_STAGE_BACKGROUND
sta irqClearStage
rts

@serviceClearBackground:
lda #PICTURE_CODE_OVERFLOW_BANK
sta RAM_BANK
jsr _b8ClearBackgroundSlice
beq @serviceWorkReturn
lda #IRQ_CLEAR_STAGE_PRIORITY
sta irqClearStage
@serviceWorkReturn:
rts

@serviceDisplayText:
lda #TEXT_BANK
sta RAM_BANK
lda irqTextStage
beq @serviceDisplayTextCopy
jsr b3SetPaletteSlice
beq @serviceWorkReturn
bra @finishDisplayText

@serviceDisplayTextCopy:
jsr handleDisplayTextSlice
beq @serviceWorkReturn
lda _b3PaletteRows
beq @finishDisplayText
lda #IRQ_TEXT_STAGE_PALETTE
sta irqTextStage
rts

@finishDisplayText:
lda #$1
sta _inputLineDirty
stz irqWorkKind
inc irqCompletionGeneration
bne @serviceWorkReturn
inc irqCompletionGeneration + 1
bra @serviceWorkReturn

@resetSetIrqState:
inc irqCompletionGeneration
bne @resetCommandState
inc irqCompletionGeneration + 1
@resetCommandState:
lda #IRQ_CMD_DONTCHANGE
sta sendIrqCommand

@vSyncCounter:
inc _vSyncCounter
bne @resetLatches
inc _vSyncCounter + 1
bne @resetLatches
inc _vSyncCounterHigh
bne @resetLatches
inc _vSyncCounterHigh + 1

@resetLatches:
lda #(VSYNC_BIT)
sta VERA_isr ; reset latches

@defaultIqr:
lda @previousRamBank
sta RAM_BANK
jmp (default_irq_vector)
; RTI will happen after jump

@jmpTableIrq: ;In order of IRQ_CMDS
.addr @noCommand
.addr @blankScreen
.addr @textOnly
.addr @normal
.addr @displayText
.addr @l12Only
.addr @clear
.addr @normal ;Graphics command goes to the same place as normal, it just clears while normal does not
.addr @showObj
@jmpTableIrqEnd:


@jmpTableBank: .byte $0, $0, $0, $0, TEXT_BANK, $0, $0, $0, $0 ;In order of IRQ_CMDS
@jmpTableBankEnd:
.assert (@jmpTableIrqEnd - @jmpTableIrq) = 2 * (@jmpTableBankEnd - @jmpTableBank), error, "IRQ dispatch tables must match"
@previousRamBank: .byte $0

; Global aliases end ca65's cheap-local scope, so define this after the handler.
shakeCount = @shakeCountLoad + 1
.assert shakeCount < $9F00, lderror, "Shake operand must stay in resident RAM"

.endif ; IRQ_INC
