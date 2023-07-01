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


.segment "BANKRAM07"
_b7InitIrq:
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
vSyncCounter: .byte $0
custom_irq_handler:   
   ; continue to default IRQ handler
   lda VERA_isr
   and #VSYNC_BIT
   beq defaultIqr
   lda #$11 ;Reenable the display after update
   sta VERA_dc_video
   inc vSyncCounter
   defaultIqr:
   jmp (default_irq_vector)
   ; RTI will happen after jump

.endif ; IRQ_INC