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
.import _inputLineDisplayed
.import _inputLineWasDisplayed

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
_b3TextModeTileByte: .byte TILE_BYTE_2
_b3TextBackground: .byte $0

DISPLAY_TEXT_SLICE_BYTES = 512
DISPLAY_PALETTE_SLICE_ROWS = 32

; These cursors survive the other IRQ work between bounded slices.
displayTextSourcePointer: .word $0
displayTextVeraAddress: .res 3
displayTextPagesRemaining: .byte $0
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

; At most one 40-character row: finish before yielding another game command.
.export _b3DisplayShortText
_b3DisplayShortText:
jsr _b6EnterForegroundVera
jsr b3InitDisplayTextSlice
jsr handleDisplayTextSlice
jsr b3SetPaletteSlice
lda _inputLineDisplayed
beq @shortTextComplete
lda #1
sta _inputLineDirty
@shortTextComplete:
inc irqCompletionGeneration
bne :+
inc irqCompletionGeneration + 1
:
jmp _b6LeaveForegroundVera

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
.assert (TILE_LAYER_WIDTH .mod 8) = 0, error, "Palette row must contain whole eight-cell groups"
ldx #(TILE_LAYER_WIDTH / 8)
@paletteByte:
.repeat 8
sta VERA_data0
.endrepeat
dex
bne @paletteByte

dec displayPaletteRowsRemaining
dec displayPaletteRowsThisSlice
bne @paletteRow

; VERA has advanced through the whole slice; persist its 17-bit cursor once.
lda VERA_addr_low
sta displayPaletteCursor
lda VERA_addr_high
sta displayPaletteCursor + 1
lda VERA_addr_bank
and #$1
sta displayPaletteCursor + 2

lda displayPaletteRowsRemaining
beq @paletteComplete
lda #$0
rts
@paletteComplete:
lda #$1
rts

;void b3InitLayer1MapBase(byte tileByte)
_b3InitLayer1Mapbase:
tay
SET_VERA_ADDRESS_IMMEDIATE (MAP_BASE + MENU_BAR_WIDTH * 2), #$0, #$1

ldx #(((TILE_LAYER_NO_TILES - MENU_BAR_WIDTH) .mod 256) / 4)
lda #TRANSPARENT_CHAR

; The low-byte remainder is followed by exactly the full pages in the count.
cpx #$0
beq @remainderBytes
@remainder:
.repeat 4
sta VERA_data0
sty VERA_data0
.endrepeat
dex
bne @remainder

@remainderBytes:
.repeat ((TILE_LAYER_NO_TILES - MENU_BAR_WIDTH) .mod 4)
sta VERA_data0
sty VERA_data0
.endrepeat
@fullPages:
lda #> (TILE_LAYER_NO_TILES - MENU_BAR_WIDTH)
beq @mapClearDone
sta ZP_TILE_LAYER_NO_TILES_HIGH
lda #TRANSPARENT_CHAR
@page:
ldx #64
@pageLoop:
.repeat 4
sta VERA_data0
sty VERA_data0
.endrepeat
dex
bne @pageLoop
dec ZP_TILE_LAYER_NO_TILES_HIGH
bne @page
@mapClearDone:
stz _inputLineWasDisplayed
lda #$1
sta _inputLineDirty
rts

handleDisplayTextSlice:
SET_VERA_ADDRESS_ABSOLUTE displayTextVeraAddress, #$0, #$1
ldx displayPaletteValue

lda displayTextSourcePointer
sta ZP_DISPLAY_TEXT_SOURCE_POINTER
lda displayTextSourcePointer + 1
sta ZP_DISPLAY_TEXT_SOURCE_POINTER + 1
 .assert (DISPLAY_TEXT_SLICE_BYTES .mod 256) = 0, error, "Text slices must contain whole 256-byte groups"
lda #(DISPLAY_TEXT_SLICE_BYTES / 256)
sta displayTextPagesRemaining
ldy #$0

@nextCharacter:
lda (ZP_DISPLAY_TEXT_SOURCE_POINTER), y
beq @textComplete

cmp #NEW_LINE
bne @storeTextCharacter
jsr b3DisplayTextNewLine
bra @consumeTextCharacter ; Newline advances the VERA cursor without a tile write

@storeTextCharacter:
; Publish each glyph with its selected palette in the same copy slice.
sta VERA_data0
stx VERA_data0

@consumeTextCharacter:
iny
bne @nextCharacter
inc ZP_DISPLAY_TEXT_SOURCE_POINTER + 1
dec displayTextPagesRemaining
beq @textBudgetComplete
bra @nextCharacter

@textComplete:
lda #$1
bra @saveTextState

@textBudgetComplete:
lda #$0

@saveTextState:
tax
lda VERA_addr_low
sta displayTextVeraAddress
lda VERA_addr_high
sta displayTextVeraAddress + 1
lda VERA_addr_bank
and #$1
sta displayTextVeraAddress + 2
; Fold the index into the persisted 16-bit cursor, including source wrap.
tya
clc
adc ZP_DISPLAY_TEXT_SOURCE_POINTER
sta displayTextSourcePointer
lda ZP_DISPLAY_TEXT_SOURCE_POINTER + 1
adc #$0
sta displayTextSourcePointer + 1
txa
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

SET_VERA_ADDRESS_ABSOLUTE _displayTextAddressToCopyTo, #$0, #$1
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
ora #$0 ; A still holds the command; recompute N/Z without changing C/V.
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
SHOW_OBJ = 8
CLEAR_OBJ = 9
SET_BACKGROUND = 10
HIGHLIGHT_INVENTORY_ROW = 11
IRQ_CMD_COUNT = HIGHLIGHT_INVENTORY_ROW + 1
.assert IRQ_CMD_COUNT <= 256, error, "IRQ command index must fit in one byte"


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
_foregroundVeraDepth: .byte $0

.export _vSyncCounterHigh, _b6CopyVSyncCounter32
.export _b6EnterForegroundVera, _b6LeaveForegroundVera, _foregroundVeraDepth

; Long foreground VERA work owns the VERA ports and IRQ scratch. Keep VSYNC
; accounting and sound live while deferring normal input and display work.
_b6EnterForegroundVera:
php
sei
inc _foregroundVeraDepth
plp
rts

_b6LeaveForegroundVera:
php
sei
dec _foregroundVeraDepth
plp
rts

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
lda VERA_ctrl
sta @previousVeraCtrl

; continue to default IRQ handler
lda VERA_isr
and #VSYNC_BIT
bne @callSoundHandler
jmp @defaultIqr

@callSoundHandler:
lda _foregroundVeraDepth
beq @normalVsyncWork
jsr b6ForegroundSound
jmp @vSyncCounter

@normalVsyncWork:
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
cmp #IRQ_CMD_COUNT
bcc @commandInRange
lda #IRQ_CMD_DONTCHANGE
sta sendIrqCommand
tax
bra @dispatchCommand
@commandInRange:
tax
@dispatchCommand:

lda @jmpTableBank, x
sta RAM_BANK
txa
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

lda #DISPLAY_SCALE_GRAPHICS
sta VERA_dc_hscale
sta VERA_dc_vscale

lda sendIrqCommand
cmp #IRQ_CMD_NORMAL
bne @normalSetup
jmp @resetSetIrqState

@normalSetup:
lda #TILE_BYTE_2
TRAMPOLINE #TEXT_BANK, _b3InitLayer1Mapbase
TRAMPOLINE #GRAPHICS_BANK, _b6InitInput

jmp @resetSetIrqState

@textOnly:
stz VERA_ctrl
lda #LAYER_0_SPRITES_DISABLE_1_ENABLE
sta VERA_dc_video

; lda #$FF
; sta VERA_L1_vscroll_h

lda #DISPLAY_SCALE_TEXT_H
sta VERA_dc_hscale
lda #DISPLAY_SCALE_TEXT_V
sta VERA_dc_vscale


lda _b3TextModeTileByte
jsr _b3InitLayer1Mapbase
lda #TILE_BYTE_2
sta _b3TextModeTileByte
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
TRAMPOLINE #SHOW_OBJ_BANK, b11ShowObjIrqHandler
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
beq @serviceClear
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
lda #TILE_BYTE_2
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
; A hidden input line must not clear game text sharing its row.
lda _inputLineDisplayed
beq @displayTextComplete
sta _inputLineDirty
@displayTextComplete:
stz irqWorkKind
inc irqCompletionGeneration
bne @serviceWorkReturn
inc irqCompletionGeneration + 1
bra @serviceWorkReturn

@clearObj:
TRAMPOLINE #SHOW_OBJ_BANK, b11ClearObjIrqHandler
bra @resetSetIrqState

@setBackground:
jsr _b6SetBackgroundColor
bra @resetSetIrqState

@highlightInventoryRow:
jsr bDHighlightInventoryRow
bra @resetSetIrqState

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
lda @previousVeraCtrl
sta VERA_ctrl
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
.addr @clearObj
.addr @setBackground
.addr @highlightInventoryRow
@jmpTableIrqEnd:

@jmpTableBank:
.byte $0, $0, TEXT_BANK, $0, TEXT_BANK, $0, $0, $0, SHOW_OBJ_BANK, SHOW_OBJ_BANK, GRAPHICS_BANK, OBJECT_BANK
@jmpTableBankEnd:
.assert (@jmpTableIrqEnd - @jmpTableIrq) = 2 * (@jmpTableBankEnd - @jmpTableBank), error, "IRQ dispatch tables must match"
@previousRamBank: .byte $0
@previousVeraCtrl: .byte $0
.assert (@jmpTableIrqEnd - @jmpTableIrq) / 2 = IRQ_CMD_COUNT, error, "IRQ command table length must match command range"
.assert (@jmpTableBankEnd - @jmpTableBank) = IRQ_CMD_COUNT, error, "IRQ bank table length must match command range"

; Global aliases end ca65's cheap-local scope, so define this after the handler.
shakeCount = @shakeCountLoad + 1
.assert shakeCount < $9F00, lderror, "Shake operand must stay in resident RAM"

; Sound borrows both ports without changing an interrupted renderer or FX multiply.
b6ForegroundSound:
lda #SOUND_BANK
sta RAM_BANK
lda _bBChannelsPlaying
beq @return
lda SOUND_SREG
pha
lda SOUND_SREG + 1
pha
stz VERA_ctrl
lda VERA_addr_low
pha
lda VERA_addr_high
pha
lda VERA_addr_bank
pha
lda #1
sta VERA_ctrl
lda VERA_addr_low
pha
lda VERA_addr_high
pha
lda VERA_addr_bank
pha
lda #(2 << 1)
sta VERA_ctrl
lda VERA_fx_ctrl
pha
stz VERA_fx_ctrl
jsr bBSoundHandler
lda #(2 << 1)
sta VERA_ctrl
pla
sta VERA_fx_ctrl
lda #1
sta VERA_ctrl
pla
sta VERA_addr_bank
pla
sta VERA_addr_high
pla
sta VERA_addr_low
stz VERA_ctrl
pla
sta VERA_addr_bank
pla
sta VERA_addr_high
pla
sta VERA_addr_low
pla
sta SOUND_SREG + 1
pla
sta SOUND_SREG
@return:
rts

.endif ; IRQ_INC
