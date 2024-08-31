.segment "BANKRAM08"

.ifndef  FILL_INC

FILL_INC = 1

GENERAL_TMP = ZP_TMP_13

.include "lineDrawing.s"
.include "fillStack.s"

.import _bDbgShowPriority
.import _picColour
.import _priDrawEnabled

.macro PLOT_LINE_VARS
COLOR           = ZP_TMP_3
LENGTH_LOW      = ZP_TMP_6
Y_VAL              = ZP_TMP_7
X1_VAL          = ZP_TMP_7 + 1
X0_VAL          = ZP_TMP_8 + 1
.endmacro

color_table:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

even_color_table:
    .byte $F0, $10, $20, $30, $40, $50, $60, $70, $80, $90, $A0, $B0, $C0, $D0, $E0, $F0

mask_table:
    .byte %11111111 ; 0 $00
    .byte %11111100 ; 1 $03
    .byte %11110000 ; 2 $0F
    .byte %11000000 ; 3 $3F
    .byte %11111111; 0 $00


pri_mask_table_odd_ending:
    .byte %11111101 ; 0 $00
    .byte %11110100 ; 1 $03
    .byte %11010000 ; 2 $0F
    .byte %01000000 ; 3 $3F
    .byte %11111101; 0 $00

.ifdef DEBUG
.export _b8TestAsmPlotPriHLineFast
.proc _b8TestAsmPlotPriHLineFast
 PLOT_LINE_VARS
 
 lda #5
 sta COLOR

 ldx #$0
 ldy #$0
@loop:
 txa
 sta $ba
 
 phx
 phy
 lda #159
 jsr _b8AsmPlotPriHLineFast
 ply
 plx
 inx
 iny
 cpx #160
 bne @loop

 TRAMPOLINE _debugBank, _bDbgShowPriority

  @loopForever:
 bra @loopForever

rts
.endproc
.endif

.macro CAN_FILL X_VAL, Y_VAL, VERA_CTRL_VALUE
.scope
    ; registers X and Y contain pixel coordinates
    ; returns 0 in A register if the pixel cannot be filled (early exit)
    ; returns 1 in A register if the pixel can be filled
    VIS_PIXEL = ZP_TMP_16
    PRI_PIXEL = ZP_TMP_16 + 1

    .ifnblank VERA_CTRL_VALUE
    lda VERA_CTRL_VALUE
    sta VERA_ctrl
    .endif

    ldy Y_VAL
    ; get the vis pixel at the current x and y
    CALC_VRAM_ADDR_LINE_DRAW_160 X_VAL
    
    lda VERA_data0
    and #$0F ; mask out the top 4 bits
    sta VIS_PIXEL

    ; is priority disabled and the current vis pixel not white?
    ; if (!pri_enabled && (asm_get_vis_pixel(x, y) != 15)) return 0;
    lda _priDrawEnabled
    bne @pri_enabled_check ; if priority is enabled, skip this check
    lda VIS_PIXEL
    cmp #15
    bne cannot_fill

@pri_enabled_check:
    ; is priority enabled and vis disabled and the current pri pixel not red?
    ; if (pri_enabled && !vis_enabled && (asm_get_pri_pixel(x, y) != 4)) return 0;
    lda _picDrawEnabled
    bne vis_enabled_check
    
    CALC_VRAM_ADDR_PRIORITY_160 X_VAL
    lda X_VAL
    lsr 
    bcc @even
    
    @odd:
    lda VERA_data0
    and #$0F
    cmp #4
    bne cannot_fill
    bra vis_enabled_check

    @even:
    lda VERA_data0
    and #$F0
    cmp #$40
    bne cannot_fill
    
    @comparePriority:


vis_enabled_check:
    ; is priority enabled and the current vis pixel not white?
    ; if (pri_enabled && (asm_get_vis_pixel(x, y) != 15)) return 0;
    lda _priDrawEnabled
    beq @can_fill
    lda VIS_PIXEL
    cmp #15
    bne cannot_fill

@can_fill:
    lda #1 ; return 1 (pixel can be filled)
    ldx #0 ; clear X register
    bra end_macro

cannot_fill:
    lda #0 ; return 0 (pixel cannot be filled)

end_macro: 

.endscope
.endmacro

BACKWARD_DIRECTION = %11000
FORWARD_DIRECTION = %10000
;Turns auto increment back on after switch off
.macro SETUP_AUTO_INC direction, X_VAL, Y_VAL
.scope
.local @end
.local @incrementOn
.local @noIncrement

;Visual screen controlled with channel 0        
stz VERA_ctrl
lda direction
sta VERA_addr_bank

;Priority Screen controlled with channel 1
@checkPriority:
lda _priDrawEnabled
beq @end
lda #$1
sta VERA_ctrl
lda X_VAL
lsr 
bcc @end
@incrementOn:
lda direction
sta VERA_addr_bank

@end:
.endscope
.endmacro

.macro SETUP_AUTO_INC_CAN_FILL direction, X_VAL, Y_VAL
.scope
.local @end
.local @incrementOn
.local @noIncrement

ldy Y_VAL
CALC_VRAM_ADDR_LINE_DRAW_160 X_VAL, #$0
lda direction
sta VERA_addr_bank

lda _priDrawEnabled
beq @end
ldy Y_VAL
CALC_VRAM_ADDR_PRIORITY_160 X_VAL, #$1
lda X_VAL
lsr 
bcs @incrementOn
@noIncrement:
stz VERA_addr_bank
bra @end
@incrementOn:
lda direction
sta VERA_addr_bank

@end:
.endscope
.endmacro

.macro POST_CAN_FILL SKIP_PRIORITY_LABEL
ldx _priDrawEnabled
beq SKIP_PRIORITY_LABEL
lda VERA_addr_bank
eor #%10000
sta VERA_addr_bank
.endmacro

.macro CAN_FILL_AUTO_INCREMENT X_VAL
.scope
    ; returns 0 in A register if the pixel cannot be filled (early exit)
    ; returns 1 in A register if the pixel can be filled
    ldx VERA_data0
    ldy VERA_data1

    ; is priority disabled and the current vis pixel not white?
    ; if (!pri_enabled && (asm_get_vis_pixel(x, y) != 15)) return 0;
    lda _priDrawEnabled
    bne @pri_enabled_check ; if priority is enabled, skip this check
    cpx #$FF
    bne cannot_fill

@pri_enabled_check:
    ; is priority enabled and vis disabled and the current pri pixel not red?
    ; if (pri_enabled && !vis_enabled && (asm_get_pri_pixel(x, y) != 4)) return 0;
    lda _picDrawEnabled
    bne vis_enabled_check
    
    lda X_VAL
    lsr 
    bcc @even
    
    @odd:
    tya
    and #$0F
    cmp #4
    bne cannot_fill
    bra vis_enabled_check

    @even:
    tya
    and #$F0
    cmp #$40
    bne cannot_fill
    


vis_enabled_check:
    ; is priority enabled and the current vis pixel not white?
    ; if (pri_enabled && (asm_get_vis_pixel(x, y) != 15)) return 0;
    lda _priDrawEnabled
    beq @can_fill
    cpx #$FF
    bne cannot_fill

@can_fill:
    lda #1 ; return 1 (pixel can be filled)
    ldx #0 ; clear X register
    bra end_macro

cannot_fill:
    lda #0 ; return 0 (pixel cannot be filled)

end_macro: 

.endscope
.endmacro


;void SCAN_AND_FILL(uint8_t x, uint8_t y)
;{
;    static uint8_t lx, rx;
;
;    ;printfSafe("in. trying to fill %d, %d\n", x,y);
;
;    ; Inline can_fill logic at the start to avoid unnecessary function calls
;    if (b8AsmCanFill(x, y) == false) {
;        return;
;    }

;    lx = x;
;    rx = x;
;
;
;    ; Inline can_fill logic for left expansion
;    while (lx != 0) {
;        if (b8AsmCanFill(lx - 1, y) == false) {
;            break;
;        }
;        --lx;
;    }
;
;
;    ; Inline can_fill logic for right expansion
;    while (rx != 159) {
;        if (b8AsmCanFill(rx + 1, y) == false) {
;            
;            break;
;        }
;        ++rx;
;        ;printfSafe("l2 rx %d\n", rx);
;    }
;
;    ;printfSafe("at 3. x0 %d x1 %d y %d color %d\n", lx, rx + 1, y, picColour);
;
;    ; pset_hline(lx, rx, y);
;        
;        if (drawCounter == 84)
;        {
;            enableStop = TRUE;
;        }
;        
;        if (picDrawEnabled)
;        {
;            b8AsmPlotVisHLineFast(lx, rx, y, picColour);
;        }
;        enableStop = FALSE;
;
;    ;printfSafe("at 4\n");
;
;   /* if (priDrawEnabled)
;        asm_plot_pri_hline_fast((lx << 1), (rx << 1) + 2, y + STARTING_BYTE, priColour);*/
;
;    ; if (y != 167) {
;    ;     push(lx, rx, y + 1, 1); ; push below
;    ; }
;    ; if (y != 0) {
;    ;     push(lx, rx, y - 1, -1); ; push above
;    ; }
;
;        if (y < PICTURE_HEIGHT - 1)
;        {
;            b8Push(lx, rx, y + 1); ; push below
;        }
;
;        if (y > 0)
;        {
;            b8Push(lx, rx, y - 1); ; push above
;        }
;}
.macro SCAN_AND_FILL
.scope
.local X_VAL
.local Y_VAL
.local LX
.local RX
.local @setupLeftExpansion
.local leftExpansionLoop
.local endLeftExpansionLoop
.local rightExpansionLoop
.local rightExpansionLoopCheck
.local endRightExpansionLoop
.local cannot_fill
.local end

CALL_PLOT_LINE_VARS

X_VAL = ZP_TMP_10
Y_VAL = ZP_TMP_10 + 1
LX = ZP_TMP_12
RX = ZP_TMP_12 + 1

sty Y_VAL
stx X_VAL

CAN_FILL X_VAL, Y_VAL, #$0
cmp #$0
bne @expansion
jmp cannot_fill

@expansion:
lda X_VAL
sta LX
sta RX

;while (lx != 0)
bne @setupLeftExpansion
jmp endLeftExpansionLoop

@setupLeftExpansion:
lda LX
dec
sta GENERAL_TMP

SETUP_AUTO_INC_CAN_FILL #BACKWARD_DIRECTION, GENERAL_TMP, Y_VAL
leftExpansionLoop:

; if (b8AsmCanFill(lx - 1, y) == false break;
 
lda LX
dec
sta GENERAL_TMP
CAN_FILL_AUTO_INCREMENT GENERAL_TMP
cmp #$0
beq endLeftExpansionLoop
POST_CAN_FILL @decrementLX

@decrementLX:
;--lx
lda LX
dec
sta LX

beq endLeftExpansionLoop
jmp leftExpansionLoop
endLeftExpansionLoop:

;while (rx != 159) {
lda RX
rightExpansionLoopCheck:
cmp #PICTURE_WIDTH - 1
bne @setupRightExpansion
jmp endRightExpansionLoop

@setupRightExpansion:
lda RX
inc
sta GENERAL_TMP
SETUP_AUTO_INC_CAN_FILL #FORWARD_DIRECTION, GENERAL_TMP, Y_VAL
rightExpansionLoop:
;if (b8AsmCanFill(rx + 1, y) == false) break
lda RX
inc
sta GENERAL_TMP
nop
CAN_FILL_AUTO_INCREMENT GENERAL_TMP
cmp #$0
beq endRightExpansionLoop

POST_CAN_FILL @incrementRX

@incrementRX:
inc RX ;rx++

lda RX
cmp #PICTURE_WIDTH - 1 ;A duplicate of the while loop condition which saves a jump
beq endRightExpansionLoop

jmp rightExpansionLoop
endRightExpansionLoop:

;if (picDrawEnabled)
;        {
;            b8AsmPlotVisHLineFast(lx, rx, y, picColour);
;        }

lda _picDrawEnabled
beq @priDraw
lda LX
sta PLOT_LINE_X0_LOW
ldy Y_VAL
lda _picColour
sta PLOT_LINE_COLOR
lda RX

jsr _b8AsmPlotVisHLineFast

@priDraw:
lda _priDrawEnabled
beq @pushBelow
lda LX
sta PLOT_LINE_X0_LOW
ldy Y_VAL
lda _priColour
sta PLOT_LINE_COLOR
lda RX

jsr _b8AsmPlotPriHLineFast


;JSRFAR _b5WaitOnKey, 5

@pushBelow:
;        if (y < PICTURE_HEIGHT - 1)
;        {
;            b8Push(lx, rx, y + 1); ; push below
;        }
;
ldy Y_VAL
cpy #PICTURE_HEIGHT - 1
bcs @pushAbove
lda LX
ldx RX
iny
FILL_STACK_PUSH
@pushAbove:
;        if (y > 0)
;        {
;            b8Push(lx, rx, y - 1); ; push above
;        }
ldy Y_VAL
beq @return
lda LX
ldx RX
dey
FILL_STACK_PUSH

@return:
.import _b5WaitOnKey
jmp end
cannot_fill:
end:
.endscope
.endmacro


lsrTable:
.byte $00, $00, $00, $00, $01, $01, $01, $01, $02, $02, $02, $02, $03, $03, $03, $03
.byte $04, $04, $04, $04, $05, $05, $05, $05, $06, $06, $06, $06, $07, $07, $07, $07
.byte $08, $08, $08, $08, $09, $09, $09, $09, $0A, $0A, $0A, $0A, $0B, $0B, $0B, $0B
.byte $0C, $0C, $0C, $0C, $0D, $0D, $0D, $0D, $0E, $0E, $0E, $0E, $0F, $0F, $0F, $0F
.byte $10, $10, $10, $10, $11, $11, $11, $11, $12, $12, $12, $12, $13, $13, $13, $13
.byte $14, $14, $14, $14, $15, $15, $15, $15, $16, $16, $16, $16, $17, $17, $17, $17
.byte $18, $18, $18, $18, $19, $19, $19, $19, $1A, $1A, $1A, $1A, $1B, $1B, $1B, $1B
.byte $1C, $1C, $1C, $1C, $1D, $1D, $1D, $1D, $1E, $1E, $1E, $1E, $1F, $1F, $1F, $1F
.byte $20, $20, $20, $20, $21, $21, $21, $21, $22, $22, $22, $22, $23, $23, $23, $23
.byte $24, $24, $24, $24, $25, $25, $25, $25, $26, $26, $26, $26, $27, $27, $27, $27
.byte $28, $28, $28, $28, $29, $29, $29, $29, $2A, $2A, $2A, $2A, $2B, $2B, $2B, $2B
.byte $2C, $2C, $2C, $2C, $2D, $2D, $2D, $2D, $2E, $2E, $2E, $2E, $2F, $2F, $2F, $2F
.byte $30, $30, $30, $30, $31, $31, $31, $31, $32, $32, $32, $32, $33, $33, $33, $33
.byte $34, $34, $34, $34, $35, $35, $35, $35, $36, $36, $36, $36, $37, $37, $37, $37
.byte $38, $38, $38, $38, $39, $39, $39, $39, $3A, $3A, $3A, $3A, $3B, $3B, $3B, $3B
.byte $3C, $3C, $3C, $3C, $3D, $3D, $3D, $3D, $3E, $3E, $3E, $3E, $3F, $3F, $3F, $3F

.macro CALL_PLOT_LINE_VARS
PLOT_LINE_COLOR           = ZP_TMP_3
PLOT_LINE_X0_LOW          = ZP_TMP_8 + 1
.endmacro

; asm_plot_vis_hline(unsigned short x0, unsigned short x1, unsigned char y, unsigned char color);
; plots 2 pixels at a time for 160x200 mode
.macro PLOT_VIS_H_LINE
.scope
    PLOT_LINE_VARS

    ; Line is a short line
    lda #%00000100  ; DCSEL = Mode 2 
    sta VERA_ctrl
    stz VERA_dc_video ; Disable cache writing

    stx X1_VAL
    ; *** call the vram address calculation routine ***
    CALC_VRAM_ADDR_LINE_DRAW_160 X0_VAL


    lda #$10    ; Enable auto-increment
    sta VERA_addr_bank

    ; Calculate the line length and the loop count / 2      
    lda X1_VAL
    sec
    sbc X0_VAL
    tax

    ldy COLOR
    lda color_table, y
    
    @loop:
        ; Plotting action 
        sta VERA_data0
        dex
        bne @loop
.endscope
.endmacro ; _plot_vis_hline

shortVisLine:
PLOT_VIS_H_LINE
rts ; Return from subroutine
b8AsmPlotVisHLineJump:
jmp shortVisLine

;X1: a Y_VAL: y color: color X0: X0_VAL
.proc _b8AsmPlotVisHLineFast 
    PLOT_LINE_VARS


    ; Calculate the line length and the loop count
    ; Ensure X1 >= X0 
    inc
    tax
    sec
    sbc X0_VAL
    sta LENGTH_LOW 
    cmp #$10
    bcc b8AsmPlotVisHLineJump
long_line:
    ; Change DCSEL to mode 6 for cache write operations
    lda #%00001100
    sta VERA_ctrl

    ; *** call the vram address calculation routine ***
    CALC_VRAM_ADDR_LINE_DRAW_160 X0_VAL
    
    ldx COLOR
    ldy color_table, x
    sty $9f29
    sty $9f2A
    sty $9f2B
    sty $9f2C

    stz VERA_ctrl
    lda #%10000
    sta VERA_addr_bank

    ldx VERA_addr_low
    @nonDivideByFourLoopCheck: 
    txa
    and #3
    beq @endLoop
    sty VERA_data0
    inx
    dec LENGTH_LOW
    bra @nonDivideByFourLoopCheck
    @endLoop:

    ; Set up VERA for cache operations
    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    lda #%01000000  ; Enable cache writing
    sta VERA_dc_video

    lda LENGTH_LOW
    tax
    ldy lsrTable,x ;To Do check if we shouldn't instead by anding the result of this with 3

    and #3
    tax 

    ; Set address auto-increment to 4 bytes
    lda #%00110000
    sta VERA_addr_bank

      ; Loop counter
    lda #%0 ; clear the mask

@loop:
    ; Plotting action
    sta VERA_data0
    dey
    bne @loop 

done_plotting:
    ; Handle the last partial chunk 

    lda mask_table,x
    sta VERA_data0

    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    stz VERA_dc_video ; Disable cache writing
    
    rts                     ; Return from subroutine
.endproc ; _plot_vis_hline_fast

.macro PLOT_PRIORITY_PLOT_LINE_VARS
.scope
PLOT_LINE_VARS
PLOT_PRIORITY COLOR, X0_VAL
.endscope
.endmacro

; asm_plot_pri_hline(unsigned short x0, unsigned short x1, unsigned char y, unsigned char color);
; plots 2 pixels at a time for 160x200 mode
.macro PLOT_PRI_H_LINE
.scope

    PLOT_LINE_VARS

    ; Line is a short line
    ldx #%00000100  ; DCSEL = Mode 2 
    stx VERA_ctrl
    stz VERA_dc_video ; Disable cache writing

    lsr X0_VAL
    bcc @autoIncrement

    stz VERA_addr_bank
    tay
    lda VERA_data0
    and #$F0
    ora COLOR
    ldx #$10    ; Enable auto-increment
    stx VERA_addr_bank
    sta VERA_data0
    dey
    cpy #1
    beq @lastNibble
    tya
    bra @setColor

    @autoIncrement:
    ldy #$10    ; Enable auto-increment
    sty VERA_addr_bank

    @setColor:
    ldy COLOR
    ldx color_table, y

    @loop:
        ; Plotting action 
        stx VERA_data0
        dec
        dec
        beq @end
        cmp #$1
        bne @loop
    @lastNibble:
        stz VERA_addr_bank
        lda VERA_data0
        and #$0F
        ldy COLOR
        ora even_color_table,y
        sta VERA_data0       
    @end:
.endscope
.endmacro ; _plot_pri_hline

shortPriLine:
PLOT_PRI_H_LINE
rts ; Return from subroutine
b8AsmPlotPriHLineJump:
jmp shortPriLine
singlePriPixelJump:

PLOT_PRIORITY_PLOT_LINE_VARS

rts

;X1: a Y_VAL: y color: color X0: X0_VAL
.proc _b8AsmPlotPriHLineFast 
    
    PLOT_LINE_VARS
    
    ; Calculate the line length and the loop count
    ; Ensure X1 >= X0    
    sta X1_VAL
    
    CALC_VRAM_ADDR_PRIORITY_160 X0_VAL, #$0
    
    lda X1_VAL
    sec
    sbc X0_VAL
    inc

    cmp #1
    beq singlePriPixelJump
    cmp #$F
    bcc b8AsmPlotPriHLineJump

    lsr X0_VAL ;Half a priority lines takes half as many bytes
    bcc @length_half

    tay

    stz VERA_addr_bank
    lda VERA_data0
    and #$F0
    ora COLOR
   
    ldx #%10000
    stx VERA_addr_bank
    dey    
    sta VERA_data0

    tya

@length_half:
    lsr
    php
    sta LENGTH_LOW 
    
long_line:
    ; Change DCSEL to mode 6 for cache write operations
    lda #%00001100
    sta VERA_ctrl

    ; *** call the vram address calculation routine ***
    
    ldx COLOR
    ldy color_table, x
    sty $9f29
    sty $9f2A
    sty $9f2B
    sty $9f2C

    stz VERA_ctrl
    lda #%10000
    sta VERA_addr_bank

    ldx VERA_addr_low
    @nonDivideByFourLoopCheck: 
    txa
    and #3
    beq @endLoop
    sty VERA_data0
    inx
    dec LENGTH_LOW
    bra @nonDivideByFourLoopCheck
    @endLoop:

    ; Set up VERA for cache operations
    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    lda #%01000000  ; Enable cache writing
    sta VERA_dc_video

    lda LENGTH_LOW
    tax
    ldy lsrTable,x
    and #3
    tax 

    ; Set address auto-increment to 4 bytes
    lda #%00110000
    sta VERA_addr_bank

      ; Loop counter
    lda #%0 ; clear the mask

@loop:
    ; Plotting action
    sta VERA_data0
    dey
    bne @loop 
done_plotting:
    ; Handle the last partial chunk 

    plp
    bcs @oddEnding

    @evenEnding:
    lda mask_table,x
    bra @writeMask
    @oddEnding:
    lda pri_mask_table_odd_ending,x
    @writeMask:
    sta VERA_data0

    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    stz VERA_dc_video ; Disable cache writing
     
    rts                     ; Return from subroutine
.endproc ; _plot_pri_hline_fast

.proc _b8AsmFloodFill
    VIS_ADDRESS = ZP_TMP_14
    PRI_ADDRESS = ZP_TMP_21
    LX      = ZP_TMP_17 + 1
    RX      = ZP_TMP_18
    Y1      = ZP_TMP_18 + 1
    NX      = ZP_TMP_19 
    X_VAL   = ZP_TMP_20
    Y_VAL   = ZP_TMP_20 + 1
    sta X_VAL ; x is in A register
    jsr popa
    sta Y_VAL 

@checkCanFill:
    CAN_FILL X_VAL, Y_VAL, #$0
    cmp #$0
    bne ok_fill
    rts
ok_fill:

    ; fill_stack_pointer = 0;
    stz FILL_STACK_POINTER

    ; scan_and_fill(x, y);
    ldx X_VAL
    ldy Y_VAL
    SCAN_AND_FILL

    ; while (pop(&lx, &rx, &y1)) {
pop_loop:
    FILL_STACK_POP LX, RX, Y1
    bne @pop_not_done ; bne branches if Z flag is clear (ie not equal to zero)
    jmp pop_done
@pop_not_done:
    ; nx = lx;
    lda LX
    sta NX
    ; while (nx <= rx) {

SETUP_AUTO_INC_CAN_FILL #FORWARD_DIRECTION, NX, Y1
outer_loop_start:
    lda RX

    ;This instruction subtracts the contents of memory from the contents of the accumulator.
    ;The use of the CMP affects the following flags: 
    ; Z flag is set on an equal comparison, reset otherwise (ie M==A Z=1)
    ; the N flag is set or reset by the result bit 7, (ie M-A<0 N=1)
    ; the carry flag is set when the value in memory is less than or equal to the accumulator, (ie M<=A C=1)
    cmp NX ; compare NX to RX
    bcs @nx_less_than_rx ; bcc branches if C flag is set (ie NX <= RX)
    jmp outer_loop_end
@nx_less_than_rx:
    ; if (can_fill(nx, y1)) {
    CAN_FILL_AUTO_INCREMENT NX

    cmp #0
    bne @start_fill ; branch if can_fill returned true
    POST_CAN_FILL @skipPostCanFill  
    @skipPostCanFill:
    inc NX
    jmp outer_loop_start
    @start_fill:
    ; scan_and_fill(nx, y1);    
    ldx NX
    ldy Y1
    SCAN_AND_FILL
    SETUP_AUTO_INC_CAN_FILL #FORWARD_DIRECTION, NX, Y1
    ; while (nx <= rx && can_fill(nx, y1)) {

inner_loop_start:
    lda NX
    cmp RX
    ; bcs outer_loop_start ; bcs branches if C flag is set (ie NX > RX)
    bcc @nx_less_than_rx_inner
    jmp else_increment_nx
@nx_less_than_rx_inner:
    stz VERA_ctrl
    stz VERA_addr_bank
    lda #$1
    sta VERA_ctrl
    stz VERA_addr_bank
    CAN_FILL_AUTO_INCREMENT NX
    cmp #$0
    beq dontEnterInnerLoop
    
    SETUP_AUTO_INC_CAN_FILL #FORWARD_DIRECTION, NX, Y1

    jmp can_fill_inner
dontEnterInnerLoop:
    
    ;two

    POST_CAN_FILL @skipPostCanFillInner
    @skipPostCanFillInner:
    SETUP_AUTO_INC #FORWARD_DIRECTION, NX, Y1
    jmp outer_loop_start
    can_fill_inner:
    POST_CAN_FILL jumpInnerLoop
    ; ++nx;
    inc NX
jumpInnerLoop:
    jmp inner_loop_start
else_increment_nx:
    ; ++nx;
    inc NX
    jmp outer_loop_start
outer_loop_end:
    jmp pop_loop
pop_done:
    ;JSRFAR _b5WaitOnKey, 5
    rts
.endproc ; _asm_flood_fill

b8FillClean: ;Fills screen with one colour, for use when there is nothing on the screen

stz GENERAL_TMP
stz GENERAL_TMP + 1

ldy #$0
CALC_VRAM_ADDR_LINE_DRAW_160 #$0, #$0
lda #$10
sta VERA_addr_bank

ldy _picColour
lda color_table, y

ldx #<(PICTURE_WIDTH * PICTURE_HEIGHT)
ldy #>(PICTURE_WIDTH * PICTURE_HEIGHT)

@loop:
sta VERA_data0

@decCounter:
dex
cpx #$FF
bne @checkLoop

@highByte:
dey

@checkLoop:
cpx #$0
bne @loop
cpy #$0
bne @loop
rts


.import _fCounter
_b8AsmFloodFillSections:
.scope
DATA = ZP_TMP_22
X_VAL = ZP_TMP_9 + 1
CLEAN_PIC = ZP_TMP_23

@continue:
sta CLEAN_PIC
stx CLEAN_PIC + 1

jsr popax 
sta @bufferStatus
stx @bufferStatus + 1


 ; is the current vis colour 15 (white)?
; if (vis_colour == 15) return 0;
lda _picColour
cmp #15
bne @checkEnabled
rts

@checkEnabled:
lda _picDrawEnabled
ora _priDrawEnabled
bne @loop
rts

@loop:
@getX:
GET_NEXT_ABS DATA, @bufferStatus, #$1
sta X_VAL
cmp #$F0
bcs @return

@getY:
GET_NEXT_ABS DATA, @bufferStatus, #$1
tay
cmp #$F0
bcs @return

@checkCleanPic:
lda (CLEAN_PIC)
and _picDrawEnabled
bne @cleanPic

tya
jsr pusha
lda X_VAL
jsr _b8AsmFloodFill

jmp @loop

@cleanPic:
jsr b8FillClean
lda #$1
sta (CLEAN_PIC)
lda #$0

@return:
rts
@bufferStatus: .word $0
.endscope
.endif