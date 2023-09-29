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
IRQ_CMD_DONTCHANGE = 0
IRQ_CMD_BLACKSCREEN = 1
IRQ_CMD_LOADSCREEN = 2
IRQ_CMD_NORMAL = 3
IRQ_CMD_DISPLAY_TEXT = 4

LAYER_1_2_ENABLE = $31
LAYER_1_2_DISABLE = $1

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
beq @defaultIqr

lda sendIrqCommand
tax
lda RAM_BANK
pha

lda @jmpTableBank, x
sta RAM_BANK
txa
clc
asl
tax
jmp (@jmpTableIrq,x)


@displayText:
jsr handleDisplayText
bra @resetSetIrqState

@blankScreen:
lda #LAYER_1_2_DISABLE
sta VERA_dc_video
lda #IRQ_CMD_BLACKSCREEN
sta currentIrqState
bra @resetSetIrqState

@normal:
lda #LAYER_1_2_ENABLE
sta VERA_dc_video
lda #IRQ_CMD_NORMAL
sta currentIrqState
bra @resetSetIrqState

@loadScreen:
lda #LAYER_1_2_DISABLE
lda #IRQ_CMD_LOADSCREEN ;Need to fill this in
sta currentIrqState
bra @resetSetIrqState

@resetSetIrqState:
lda #IRQ_CMD_DONTCHANGE
sta sendIrqCommand

@vSyncCounter:
inc vSyncCounter

@defaultIqr:
pla
sta RAM_BANK
jmp (default_irq_vector)
; RTI will happen after jump

@jmpTableIrq: ;In order of IRQ_CMDS
.addr @vSyncCounter
.addr @blankScreen
.addr @loadScreen
.addr @normal
.addr @displayText

@jmpTableBank: .byte $0, $0, $0, $0, TEXT_BANK ;In order of IRQ_CMDS

.endif ; IRQ_INC

