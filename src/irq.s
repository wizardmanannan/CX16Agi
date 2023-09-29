; Check if global definitions are included, if not, include them
.ifndef  IRQ_INC
IRQ_INC = 1

.include "global.s"

.macro SEND_IRQ_COMMAND command, vSyncToCheck
sei
lda command
sta sendIrqCommand
lda vSyncCounter
sta vSyncToCheck
cli
.endmacro

.macro WAIT_FOR_NEXT_IRQ vSyncToCheck
.local @waitForBlank
@waitForBlank: ;May as well just busy wait wai will just take extra cycles, and we aren't going anywhere until the vSync happens and the counter increments
lda vSyncToCheck
cmp vSyncCounter
bne @waitForBlank

.endmacro

;Handlers
.segment "BANKRAM03"
handleDisplayText:
rts
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
   cli ; enable IRQ now that vector is properly set
rts

_b6SetAndWaitForIrqState:
sta @state
SEND_IRQ_COMMAND @state, @vSyncToCheck
WAIT_FOR_NEXT_IRQ @vSyncToCheck
rts
@state: .byte $0
@vSyncToCheck: .byte $0

.segment "CODE"
IRQ_STATE_DONTCHANGE = 0
IRQ_STATE_BLACKSCREEN = 1
IRQ_STATE_LOADSCREEN = 2
IRQ_STATE_NORMAL = 3
IRQ_STATE_DISPLAY_TEXT = 4

LAYER_1_2_ENABLE = $31

;0 Don't Change
;1 Blank Screen
;2 Load Screen
;3 Normal
;4 Display Text
sendIrqCommand: .byte $0

;As above except it will never change to 0
currentIrqState: .byte $0

vSyncCounter: .byte $0
debugVSyncCounter: .word $0
custom_irq_handler:   
; continue to default IRQ handler
lda VERA_isr
and #VSYNC_BIT
beq defaultIqr

lda sendIrqCommand
beq @vSyncCounter

;Organised by slowest not in order of enumeration
@displayText:
cmp #IRQ_STATE_DISPLAY_TEXT
bne @blankScreen
lda RAM_BANK
pha
lda #TEXT_BANK
sta RAM_BANK
jsr handleDisplayText
pla
sta RAM_BANK
lda #IRQ_STATE_DISPLAY_TEXT
sta currentIrqState

bra @resetSetIrqState

@blankScreen:
cmp #IRQ_STATE_BLACKSCREEN
bne @normal
lda #$1
sta VERA_dc_video
lda #IRQ_STATE_BLACKSCREEN
sta currentIrqState
bra @resetSetIrqState

@normal:
cmp #IRQ_STATE_NORMAL
bne @resetSetIrqState
lda #LAYER_1_2_ENABLE
sta VERA_dc_video
lda #IRQ_STATE_NORMAL
sta currentIrqState


@resetSetIrqState:
lda #IRQ_STATE_DONTCHANGE
sta sendIrqCommand

@vSyncCounter:
inc vSyncCounter

@debugCounter:
clc
lda debugVSyncCounter
adc #$1
sta debugVSyncCounter
lda #$0
sta debugVSyncCounter+1

defaultIqr:
jmp (default_irq_vector)
; RTI will happen after jump

.endif ; IRQ_INC

