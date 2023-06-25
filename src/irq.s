; Check if global definitions are included, if not, include them
.ifndef  IRQ_INC
IRQ_INC = 1

.include "global.s"

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
custom_irq_handler:
   ; continue to default IRQ handler
   lda VERA_isr
   and #VSYNC_BIT
   beq @continue

   @continue:
   jmp (default_irq_vector)
   ; RTI will happen after jump

.endif ; IRQ_INC