; Check if global definitions are included, if not, include them
.ifndef  IRQ_INC
IRQ_INC = 1

.include "global.s"

.macro WAIT_FOR_VSYNC
.local @wait
lda vSyncCounter
@wait:
wai
cmp vSyncCounter
beq @wait
.endmacro


.macro SET_AND_WAIT_FOR_IRQ_STATE state
.local @waitForBlank
sei
lda state
sta setIrqState
cli
@waitForBlank:
wai
lda currentIrqState
cmp state
bne @waitForBlank

.endmacro


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

.segment "CODE"
IRQ_STATE_DONTCHANGE = 0
IRQ_STATE_BLACKSCREEN = 1
IRQ_STATE_LOADSCREEN = 2
IRQ_STATE_NORMAL = 3
;0 Don't Change
;1 Blank Screen
;2 Load Screen
;3 Normal
setIrqState: .byte $0

;As above except it will never change to 0
currentIrqState: .byte $0

vSyncCounter: .byte $0
debugVSyncCounter: .word $0
custom_irq_handler:   
; continue to default IRQ handler
lda VERA_isr
and #VSYNC_BIT
beq defaultIqr

lda setIrqState
beq @vSyncCounter

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
lda #$11
sta VERA_dc_video
lda #IRQ_STATE_NORMAL
sta currentIrqState


@resetSetIrqState:
lda IRQ_STATE_DONTCHANGE
sta setIrqState

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