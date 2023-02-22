.ifndef  COMMAND_LOOP_INC

 DEBUG = 1

COMMAND_LOOP_INC = 1
LOGIC_ENTRY_PARAMETERS_OFFSET =  0

.include "global.s"
.include "logicCommands.s"
.include "codeWindow.s"

.import _debugPrint

ZP_PTR_LF = $02
ZP_PTR_LE = $04

.segment "CODE"
startPos: .word $0
endPos:  .word $0
stillExecuting: .byte $1
lastCodeWasNonWindow: .byte FALSE
jumpOffset: .byte $0
numArgs: .byte $0,$2,$2,$2,$2,$2,$2,$1,$1,$1,$2,$5,$1,$0,$0,$2,$5,$5,$5


.macro DEBUG_PRINT toPrint
    .ifdef DEBUG
        .ifblank toPrint
            LOAD_CODE_WIN_CODE
        .endif
        .ifnblank toPrint
            lda toPrint
        .endif
        jsr _debugPrint
    .endif
.endmacro

.macro SET_BANK_TO_CODE_BANK
lda codeBank
sta RAM_BANK
.endmacro

.macro CODE_JUMP ;requires local variables @disp, @b1 and @b2 to be in scope
    LOAD_CODE_WIN_CODE
    sta @b1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta @b2
    INC_CODE
    LEFT_SHIFT_16 @b2, #$8, @disp

    ORA_16 @b1, @disp, @disp 
    INC_CODE_BY @disp
.endmacro

;ifHelpers
 closingIfBracket:
            INC_CODE
            INC_CODE
            jmp endifFunction

checkOrMode:
    lda orMode
    bne @orModeFalse

    jmp endIfHandlerLoop
    
    @orModeFalse:
        lda #TRUE
        sta orMode
        jmp ifHandlerLoop

toggleNotMode:
    lda notMode
    beq @toggleOn
    lda #FALSE
    sta notMode
    jmp ifHandlerLoop
    @toggleOn:
    lda #TRUE
    sta notMode
    jmp ifHandlerLoop

startOrModeLoop:
bra @start
    @ch: .byte $0
    @start:
    LOAD_CODE_WIN_CODE
    sta @ch
    INC_CODE
    
    lda @ch
    cmp #$FC

    @eqFC:
    bne @gtFC
    jmp ifHandlerLoop

    @gtFC:
    LESS_THAN_OR_EQ_8 #$FC, @ch, startOrModeLoop


    cmp #$0E
    bne @else

    @0E:
    LEFT_SHIFT_16 @ch, #$1, @ch 

    INC_CODE_BY @ch
    jmp startOrModeLoop

    @else:
        ADD_WORD_16_8 numArgs, @ch, ZP_TMP
        INC_CODE_BY @ch

;endIfHelpers
ifHandler:
        stz notMode
        stz orMode
        jmp ifHandlerLoop
        notMode: .byte FALSE
        orMode: .byte FALSE
        ch: .byte $0
        ifHandlerLoop:
        LOAD_CODE_WIN_CODE
        sta ch
        INC_CODE

        lda ch

        cmp #$FF
        beq @closingIfBracketJmp

        cmp #$fd
        beq @toggleNotModeJmp

        cmp #$FC
        beq @checkOrModeJmp

        bra @default

        @closingIfBracketJmp:
            DEBUG_PRINT ch
            jmp closingIfBracket
        
        @toggleNotModeJmp:
            DEBUG_PRINT ch
            jmp toggleNotMode

        @checkOrModeJmp:
            DEBUG_PRINT ch
            jmp checkOrMode

        @default:
            DEBUG_PRINT ch
            lda ch
            asl
            sta jumpOffset
            
            LDA #LOGIC_COMMANDS_BANK
            sta RAM_BANK
            ldx jumpOffset
            jmp (jmpTableIf,x)
            
            returnFromOpCodeFalse:
                SET_BANK_TO_CODE_BANK
                lda notMode
                bne returnFromOpCodeTrueAfterNotMode            
                
                returnFromOpCodeFalseAfterNotMode:

                SET_BANK_TO_CODE_BANK
                lda orMode      
                
                beq endIfHandlerLoop
                jmp ifHandlerLoop  

            returnFromOpCodeTrue:
                SET_BANK_TO_CODE_BANK
                lda notMode
                bne returnFromOpCodeFalseAfterNotMode

            returnFromOpCodeTrueAfterNotMode:
                lda orMode
                beq @gotoStartIfHandler
                jmp startOrModeLoop
                @gotoStartIfHandler:
                jmp ifHandlerLoop
    
            endIfHandlerLoop:
                    bra @startFindBracketLoop
                    @ch: .byte $0
                    @b1: .word $0
                    @b2: .word $0
                    @disp: .word $0
                    @startFindBracketLoop:
                        LOAD_CODE_WIN_CODE
                        sta @ch
                        
                        INC_CODE
                        lda @ch
                        
                        cmp #$FF
                        beq @FFResult

                        cmp #$0E
                        beq @0EResult

                        GREATER_THAN_OR_EQ_8 @ch, #$FC, @startFindBracketLoop

                        ldx @ch
                        lda numArgs,x
                        sta @disp

                        ADD_WORD_16 ZP_PTR_CODE, @disp, ZP_PTR_CODE
                        bra @startFindBracketLoop

                        @0EResult:
                        LOAD_CODE_WIN_CODE
                        sta @ch
                        INC_CODE
                        LEFT_SHIFT_16 @ch, #$1, @disp
                        ADD_WORD_16 ZP_PTR_CODE, @disp, ZP_PTR_CODE

                        jmp @startFindBracketLoop
                        @FFResult:
                        CODE_JUMP
                        bra endifFunction

                    endifFunction:
                        jmp mainLoop

;commandLoopHelpers
goto:
    DEBUG_PRINT
    bra @start
    @b1: .byte $0
    @b2: .byte $0
    @disp: .byte $0
    @start:
    INC_CODE
    CODE_JUMP
    jmp mainLoop

;endCommandLoopHelpers

_commandLoop:
         jmp start
         entryPoint: .word $0
         codeSize: .word $0
         codeAtTimeOfLastBankSwitch: .byte $0

         start:
         sta   ZP_PTR_LF
         stx   ZP_PTR_LF  + 1

         lda   GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET
         ldx   GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET + 1
         sta   ZP_PTR_LE
         stx   ZP_PTR_LE  + 1
        
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_OFFSET, ZP_PTR_LF, startPos
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET, ZP_PTR_LF, codeSize
         GET_STRUCT_8 LOGIC_FILE_LOGIC_BANK_OFFSET, ZP_PTR_LF, codeBank
         GET_STRUCT_16 LOGIC_ENTRY_POINT_OFFSET, ZP_PTR_LE, entryPoint
         
         ADD_WORD_16 startPos,entryPoint,ZP_PTR_CODE
         ADD_WORD_16 startPos,codeSize,endPos
         
         jsr refreshCodeWindow
         mainLoop:

         GREATER_THAN_OR_EQ_16 ZP_PTR_CODE, endPos, endMainLoop
         lda stillExecuting
         cmp #TRUE
         beq @loopConditionSuccess
         jmp endMainLoop
         @loopConditionSuccess:
        SUB_WORD_16_IND ZP_PTR_CODE, startPos, LOGIC_ENTRY_CURRENT_POINT_OFFSET, ZP_PTR_LE
        ; /* Emergency exit */
		; if (key[KEY_F12]) {
		; 	////lprintf("info: Exiting MEKA due to F12, logic: %d, posn: %d",
		; 		//logNum, currentLogic.currentPoint);
		; 	exit(0);
		; }
        LOAD_CODE_WIN_CODE
        sta codeAtTimeOfLastBankSwitch
        cmp #$FF
        bne @checkGoTo
        DEBUG_PRINT
        INC_CODE
        jmp ifHandler
        bra mainLoop
        @checkGoTo:
        cmp #$FE
        bne @default
        DEBUG_PRINT
        jmp goto
        jmp mainLoop
        @default:     
            DEBUG_PRINT
            LOAD_CODE_WIN_CODE
            tax
            ldy codeBankArray,x
            sty RAM_BANK
            
            cmp #$80
            bcs @commands2
            @commands1:
            asl
            sta jumpOffset
            INC_CODE
            ldx jumpOffset
            jmp (jmpTableCommands1,x)

            @commands2:
            sec
            sbc #$80 
            asl
            sta jumpOffset
            INC_CODE
            ldx jumpOffset
            jmp (jmpTableCommands2,x)

            _afterLogicCommand:
            SET_BANK_TO_CODE_BANK

            jmp mainLoop
        endMainLoop:
        rts
.endif