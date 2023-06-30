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
hasInitedPictureVRAM: .byte $0

BITMAP_WIDTH = 320
BITMAP_HEIGHT = 240
initPictureVRAM:

lda #$6   ; Bitmap mode 16 colors
sta VERA_L0_config

stz VERA_ctrl
lda #$10
sta VERA_addr_bank
stz VERA_addr_high
stz VERA_addr_low



; Calculate number of bytes per row (BITMAP_WIDTH / 2) and store it into X
lda #<(BITMAP_WIDTH / 2)
tax

; Calculate number of rows (BITMAP_HEIGHT) and store it into @mapHeight
lda #<BITMAP_HEIGHT
sta @mapHeight

@loopOuter:
    ldy @mapHeight  ; Load Y with mapHeight
    @loopInner:
        lda #$CC
        stz VERA_data0  ; Store 0 into VRAM (set pixel to black)
        dey  ; Decrement Y
        bne @loopInner  ; If Y is not 0, continue loop
    dex  ; Decrement X
    bne @loopOuter  ; If X is not 0, continue loop
bra afterInit
@mapHeight: .byte $0

custom_irq_handler:   
   ; continue to default IRQ handler
   lda VERA_isr
   and #VSYNC_BIT
   beq defaultIqr

   lda hasInitedPictureVRAM
   bne afterInit
   bra initPictureVRAM
   inc hasInitedPictureVRAM
   afterInit:

   defaultIqr:
   jmp (default_irq_vector)
   ; RTI will happen after jump

.endif ; IRQ_INC