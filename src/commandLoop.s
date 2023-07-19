; Define the include guard for command loop
.ifndef COMMAND_LOOP_INC

; Set the value of include guard and define constants
COMMAND_LOOP_INC = 1
LOGIC_ENTRY_PARAMETERS_OFFSET = 0


;VERBOSE_SCRIPT_START = 1

.ifdef VERBOSE_SCRIPT_START
.import _b5DebugPrintScriptStart
.endif

VERBOSE_ROOM_CHANGE = 1
.ifdef VERBOSE_ROOM_CHANGE
.import _b5DebugPrintRoomChange
.endif

; Include necessary files
.include "global.s"
.include "logicCommands.s"
.include "codeWindow.s"

.global endMainLoop
.global mainLoop
.global returnFromOpCodeTrue
.global returnFromOpCodeFalse

; Set the segment for the code
.segment "CODE"
; Import required functions
.import _debugPrint
.import _b6LoadLogicFile
; Import required C variables
.import _logicEntryAddressesLow
.import _logicEntryAddressesHigh
.import _currentLog

; Define variables
jumpOffset: .byte $0
numArgs: .byte $0,$2,$2,$2,$2,$2,$2,$1,$1,$1,$2,$5,$1,$0,$0,$2,$5,$5,$5

; Define the codeBankArray, which stores the bank of every opcode (excluding return and boolean operators like greater than) starting from opcode 1 and going to 182
codeBankArray: .byte $5,$1,$1,$1,$1,$1,$1,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$1,$1,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$4,$4,$4,$4,$4,$4,$4,$4,$1,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$1,$1,$1,$4,$4,$4,$4,$4,$4,$4,$1,$4,$1,$1,$1,$1,$4,$1,$1,$1,$4,$4,$4,$4,$1,$1,$4,$4,$4,$4,$1,$4,$5,$1,$1,$1,$5,$5,$1,$1,$5,$5,$5,$5,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1

; Import debug functions if DEBUG is defined
.ifdef DEBUG
.import _debugPrintTrue
.import _debugPrintFalse
.import _debugPrintNot
.import _debugPrintOrMode
.import _codeJumpDebug
.import _opCounter
.import _stopAtFunc
.endif

; Import more required functions
.import _bFGotoFunc
.import _callC1
.import _callC2

; Logic array imports
.import _logics

; Define DEBUG_JUMP macro
.macro DEBUG_JUMP val1, val2
.ifdef DEBUG
lda val1
sta _logDebugVal1

lda val2
sta _logDebugVal2

JSRFAR _codeJumpDebug, DEBUG_BANK

.endif
.endmacro

; Debugging macros for printing true, false, and not values
.macro DEBUG_PRINT_TRUE
.ifdef DEBUG
    JSRFAR _debugPrintTrue, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_PRINT_FALSE
.ifdef DEBUG
    JSRFAR _debugPrintFalse, DEBUG_BANK  
.endif
.endmacro

.macro DEBUG_PRINT_NOT
.ifdef DEBUG
    lda notMode
    beq @exit
    JSRFAR _debugPrintNot, DEBUG_BANK  
    @exit:
.endif
.endmacro

; Debugging macro for printing the value of OR mode
.macro DEBUG_PRINT_OR_MODE
.ifdef DEBUG
    lda orMode
    beq @exit
    JSRFAR _debugPrintOrMode, DEBUG_BANK
    @exit:
.endif
.endmacro

; Debugging macro for printing a value
.macro DEBUG_PRINT toPrint
    .ifdef DEBUG
        .ifblank toPrint
            LOAD_CODE_WIN_CODE
        .endif
        .ifnblank toPrint
            lda toPrint
        .endif
        jsr debugPrintTrampoline
    .endif
.endmacro

; Macro to set the RAM bank to the code bank
.macro SET_BANK_TO_CODE_BANK
lda codeBank
sta RAM_BANK
.endmacro

; Macro to set the RAM bank to the IF bank
.macro SET_BANK_TO_IF_BANK
lda #LOGIC_COMMANDS_BANK
sta RAM_BANK
.endmacro

; Macro to handle GOTO logic
.macro GOTO
; code++;
; b1 = *code++;
; b2 = *code++;
; disp = (b2 << 8) | b1;  /* Should be signed 16 bit */
; code += disp;
    LOAD_CODE_WIN_CODE
    sta ZP_PTR_B1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta ZP_PTR_B2
    INC_CODE

    DEBUG_JUMP ZP_PTR_B1, ZP_PTR_B2
    LEFT_SHIFT_BY_8 ZP_PTR_B2, ZP_PTR_DISP

    ORA_16 ZP_PTR_B1, ZP_PTR_DISP, ZP_PTR_DISP

    INC_CODE_BY ZP_PTR_DISP
    DEBUG_CODE_STATE
.endmacro

; Debug print trampoline function
debugPrintTrampoline:
ldx RAM_BANK
phx
ldx #DEBUG_BANK
stx RAM_BANK
jsr _debugPrint
pla
sta RAM_BANK
rts

; Helper functions for if statement processing
.segment "BANKRAM0F"
closingIfBracket:
    INC_CODE ; * data += 2;
    INC_CODE
    jmp endifFunction
checkOrMode:
    lda orMode
    beq @orModeFalse                  ;if (orMode) {
                                        ;If we have reached the closing OR bracket, then the
                                        ;test for the whole expression must be false. */
                                        ;stillProcessing = FALSE;
                                        ;}
                                        ;else {
                                        ;	orMode = TRUE;
                                        ;}

    jmp endIfHandlerLoop
    
    @orModeFalse:
        lda #TRUE
        sta orMode
        jmp ifHandlerLoop

toggleNotMode:
    lda notMode  
    beq @toggleOn  ;notMode = (notMode ? FALSE : TRUE);
    lda #FALSE
    sta notMode
    jmp ifHandlerLoop
    @toggleOn:
    lda #TRUE
    sta notMode
    jmp ifHandlerLoop

;Find the closing OR. It can't just search for 0xfc
;because this could be a parameter for one of the test
;commands rather than being the closing OR. We therefore
;have to jump over each command as we find it. 

;while (TRUE) {
;						ch = *(*data)++;
;						if (ch == 0xfc) break;
;						if (ch > 0xfc) continue;
;						if (ch == 0x0e) { /* said() has variable number of args */
;							ch = *(*data)++;
;
;							*data += (ch << 1);
;						}
;						else {
;							*data += testCommands[ch].numArgs;
;						}
;					}


startOrModeLoop:
bra @start
    @ch: .word $0
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
    LEFT_SHIFT_BY_1 @ch, @ch 

    INC_CODE_BY @ch
    jmp startOrModeLoop

    @else:
        ADD_WORD_16_8 numArgs, @ch, ZP_TMP
        INC_CODE_BY @ch

;endIfHelpers
ifHandler:
        stz notMode
        stz orMode
        bra ifHandlerLoop
        notMode: .byte FALSE
        orMode: .byte FALSE
        ifHandlerLoop:
        LOAD_CODE_WIN_CODE
        sta ZP_PTR_CH
        INC_CODE

        lda ZP_PTR_CH

        cmp #$FF ;Closing if bracket. Expression must be true.
        beq @closingIfBracketJmp

        cmp #$fd ;Not mode toggle
        beq @toggleNotModeJmp

        cmp #$FC ;Or Mode
        beq @checkOrModeJmp
        

        bra @default

        @closingIfBracketJmp:
            DEBUG_PRINT ZP_PTR_CH ;/* Closing if bracket. Expression must be true. */
            jmp closingIfBracket
        
        @toggleNotModeJmp:
            DEBUG_PRINT ZP_PTR_CH
            jmp toggleNotMode

        @checkOrModeJmp:
            DEBUG_PRINT ZP_PTR_CH
            DEBUG_PRINT_OR_MODE
            jmp checkOrMode

        @default:
            DEBUG_PRINT ZP_PTR_CH
            lda ZP_PTR_CH
            asl
            sta jumpOffset
            
            LDA #LOGIC_COMMANDS_BANK
            sta RAM_BANK
            ldx jumpOffset
            jmp (jmpTableIf,x)
            
            returnFromOpCodeFalse:
                DEBUG_PRINT_FALSE 
                lda notMode ;If not mode branch to false result
                bne returnFromOpCodeTrueAfterNotMode            
                
                returnFromOpCodeFalseAfterNotMode:
                DEBUG_PRINT_NOT
                stz notMode
                lda orMode  ;if (!orMode) stillProcessing = FALSE;
                
                beq endIfHandlerLoop
                jmp ifHandlerLoop  

            returnFromOpCodeTrue:
                DEBUG_PRINT_TRUE
                lda notMode ;If not mode branch to true result
                bne returnFromOpCodeFalseAfterNotMode

            returnFromOpCodeTrueAfterNotMode:
                DEBUG_PRINT_NOT
                stz notMode
                lda orMode
                beq @gotoStartIfHandler
                jmp startOrModeLoop ;On return from a true result use the or mode loop to skip the other side of the or. Short circuit evaluation
                @gotoStartIfHandler:
                jmp ifHandlerLoop
    
            endIfHandlerLoop:
        ;while (TRUE) {
        ;    ch = *(*data)++;
        ;    if (ch == 0xff) {
        ;        b1 = *(*data)++;
        ;        b2 = *(*data)++;
        ;        disp = (b2 << 8) | b1;  /* Should be signed 16 bit */
        ;        *data += disp;
        ;        break;
        ;    }
        ;    if (ch >= 0xfc) continue;
        ;    if (ch == 0x0e) {
        ;        ch = *(*data)++;
        ;        *data += (ch << 1);
        ;    }
        ;    else {
        ;        *data += testCommands[ch].numArgs;
        ;    }
	    ;}

                    @startFindBracketLoop:
                        LOAD_CODE_WIN_CODE                                 
                        sta ZP_PTR_CH
                        
                        INC_CODE
                        lda ZP_PTR_CH
                        
                        cmp #$FF ;Closing If Bracket
                        bne @0E
                        
                        jmp @FFResult

                        @0E:
                        cmp #$0E
                        beq @0EResult

                        GREATER_THAN_OR_EQ_8 ZP_PTR_CH, #$FC, @startFindBracketLoop ;Skip or modes

                        ldx ZP_PTR_CH ;*data += testCommands[ch].numArgs;
                        lda numArgs,x
                        ldx #$0
                        sta ZP_PTR_DISP
                        stz ZP_PTR_DISP + 1

                        INC_CODE_BY ZP_PTR_DISP
                        bra @startFindBracketLoop

                        @0EResult: ;said() has variable number of args
                        LOAD_CODE_WIN_CODE
                        sta ZP_PTR_CH
                        INC_CODE
                        LEFT_SHIFT_BY_1 ZP_PTR_CH, ZP_PTR_DISP
                        sta ZP_PTR_DISP
                        INC_CODE_BY ZP_PTR_DISP

                        jmp @startFindBracketLoop
                        @FFResult:
                        GOTO
                        bra endifFunction

                    endifFunction:
                        jmp mainLoop
;endCommandLoopHelpers

.segment "CODE"

.macro STORE_LOGIC_ENTRY_ADDRESS
.local @low
.local @end
    ldx #LOGIC_ENTRY_ADDRESSES_BANK
    stx RAM_BANK
    
    cmp #$80
    bcc @low
    sec
    sbc #$80
    bra @high

@low:
  READ_ARRAY_POINTER ZP_PTR_PLF_LOW
  bra @end
@high:
  READ_ARRAY_POINTER ZP_PTR_PLF_HIGH
@end:
.endmacro

_executeLogic:
         bra start
         entryPoint: .word $0
         codeSize: .word $0
         codeAtTimeOfLastBankSwitch: .byte $0
         previousRamBank: .byte $0
         .ifdef VERBOSE_ROOM_CHANGE
         lastRoom: .byte $0
         currentRoom: .byte $0
         .endif
         start:      
         ldy RAM_BANK
         sty previousRamBank

         sta _currentLog
         stx _currentLog + 1

         STORE_LOGIC_ENTRY_ADDRESS
    
         .ifdef VERBOSE_SCRIPT_START
            JSRFAR _b5DebugPrintScriptStart, DEBUG_BANK
         .endif

         .ifdef VERBOSE_ROOM_CHANGE
            GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, currentRoom, #$0
            lda currentRoom
            cmp lastRoom
            beq @endif
            JSRFAR _b5DebugPrintRoomChange, DEBUG_BANK
            @endif:
            lda currentRoom
            sta lastRoom
         .endif

        ;if (!currentLogic.loaded) {		
		    ;trampoline_1Int(&b8LoadLogicFile, logNum, LOGIC_CODE_BANK);
        ;}
         ldx #LOGIC_BANK
         stx RAM_BANK

         GET_STRUCT_8 LOGIC_LOADED_OFFSET, ZP_PTR_LE, ZP_TMP   
         lda ZP_TMP
         bne @endifLoaded
         lda #LOGIC_CODE_BANK
         sta RAM_BANK
         
         lda _currentLog
         ldx _currentLog + 1
         jsr _b6LoadLogicFile
         
         ldx #LOGIC_BANK
         stx RAM_BANK
         @endifLoaded:    
         GET_STRUCT_16 LOGIC_FILE_LOGIC_DATA_OFFSET, ZP_PTR_LE, ZP_PTR_LF ;
         
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_OFFSET, ZP_PTR_LF, startPos ;Retrieving the struct members required
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET, ZP_PTR_LF, codeSize

         GET_STRUCT_8 LOGIC_FILE_LOGIC_BANK_OFFSET, ZP_PTR_LF, _codeBank
         
         GET_STRUCT_16 LOGIC_ENTRY_POINT_OFFSET, ZP_PTR_LE, entryPoint
         
         ADD_WORD_16 startPos,entryPoint,ZP_PTR_CODE ;code = startPos + currentLogic.entryPoint;
         ADD_WORD_16 startPos,codeSize,endPos ;startPos + currentLogicFile.codeSize;

         LDA #TRUE
         sta codeWindowInvalid ;At the beginning start from ZP_PTR_CODE
         jsr refreshCodeWindow
         mainLoop:
         GREATER_THAN_OR_EQ_16 ZP_PTR_CODE, endPos, endMainLoop
         
         lda #LOGIC_BANK
         sta RAM_BANK        
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
         SET_BANK_TO_IF_BANK
         jmp ifHandler
         bra mainLoop
         @checkGoTo:
         cmp #$FE
         beq @FE
         jmp @default
         @FE:
         DEBUG_PRINT
         DEBUG_CODE_STATE
         INC_CODE
         lda #COMMAND_LOOP_HELPER_BANK
         sta RAM_BANK
         GOTO
         jmp mainLoop
         @default:
            DEBUG_PRINT
            LOAD_CODE_WIN_CODE
            tax
            ldy codeBankArray,x ;Get the code bank from the large array
            sty RAM_BANK                
            cmp #$80
            bcs @commands2 ;Greater than 80 command is in second jump table
            @commands1:
            asl ;Multiple jump offset by 2 as addresses are two bytes each
            sta jumpOffset
            INC_CODE
            ldx jumpOffset
            jmp (jmpTableCommands1,x) ; Jump to offset

            @commands2:
            sec
            sbc #$80 
            asl
            sta jumpOffset
            INC_CODE
            ldx jumpOffset
            jmp (jmpTableCommands2,x)
        endMainLoop:
        lda previousRamBank
        sta RAM_BANK
        rts
.endif

