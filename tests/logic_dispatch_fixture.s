LOGIC_DISPATCH_TEST_HOOK = 1
.importzp sreg
.import pusha
.import _top_start, _top_end, _top_pc, _top_entry_ptr, _top_file_ptr
.import _top_code_bank, _top_ram_bank, _top_window_invalid, _top_window_first
.import _expected_a_start, _expected_a_end, _expected_a_pc, _expected_a_entry_ptr, _expected_a_file_ptr
.import _expected_a_code_bank, _expected_a_ram_bank, _expected_a_window_invalid, _expected_a_window_first
.import _expected_b_start, _expected_b_end, _expected_b_pc, _expected_b_entry_ptr, _expected_b_file_ptr
.import _expected_b_code_bank, _expected_b_ram_bank, _expected_b_window_invalid, _expected_b_window_first
.import _observed_a_start, _observed_a_end, _observed_a_pc, _observed_a_entry_ptr, _observed_a_file_ptr
.import _observed_a_code_bank, _observed_a_ram_bank, _observed_a_window_invalid, _observed_a_window_first
.import _observed_b_start, _observed_b_end, _observed_b_pc, _observed_b_entry_ptr, _observed_b_file_ptr
.import _observed_b_code_bank, _observed_b_ram_bank, _observed_b_window_invalid, _observed_b_window_first
.import _b5GetLogicFile, _b5GetLogicEntry, _b6LoadLogicFile, _b5SetLogicEntry
.import _bFGotoFunc, _callC1, _callC2, _logicEntryAddressesLow, _logicEntryAddressesHigh, _logics
.macro TRAMPOLINE BANK, JMPTO
    jsr JMPTO
.endmacro
.include "commandLoop.s"

.macro SAVE_STATE startTarget, endTarget, pcTarget, entryTarget, fileTarget, bankTarget, ramTarget, invalidTarget, firstTarget
    ADD_WORD_16 ZP_PTR_CODE, cwCurrentCode, testPC
    lda startPos
    sta startTarget
    lda startPos + 1
    sta startTarget + 1
    lda endPos
    sta endTarget
    lda endPos + 1
    sta endTarget + 1
    lda testPC
    sta pcTarget
    lda testPC + 1
    sta pcTarget + 1
    lda ZP_PTR_LE
    sta entryTarget
    lda ZP_PTR_LE + 1
    sta entryTarget + 1
    lda ZP_PTR_LF
    sta fileTarget
    lda ZP_PTR_LF + 1
    sta fileTarget + 1
    lda _codeBank
    sta bankTarget
    lda RAM_BANK
    sta ramTarget
    lda codeWindowInvalid
    sta invalidTarget
    lda codeWindow
    sta firstTarget
.endmacro

.segment "CODE"
.global _test_nested_dispatch
.global _test_snapshot_machine_state
.global _test_capture_machine_state

_test_nested_dispatch:
    lda #<$A300
    sta startPos
    lda #>$A300
    sta startPos + 1
    lda #<$A380
    sta endPos
    lda #>$A380
    sta endPos + 1
    lda #<$A300
    sta ZP_PTR_CODE
    lda #>$A300
    sta ZP_PTR_CODE + 1
    lda #2
    sta cwCurrentCode
    stz cwCurrentCode + 1
    lda #$12
    sta ZP_PTR_LE
    lda #$34
    sta ZP_PTR_LE + 1
    lda #$78
    sta ZP_PTR_LF
    lda #$56
    sta ZP_PTR_LF + 1
    lda #43
    sta _codeBank
    lda #$39
    sta RAM_BANK
    stz codeWindowInvalid
    lda #0
    jsr callLogic

    SAVE_STATE _top_start, _top_end, _top_pc, _top_entry_ptr, _top_file_ptr, _top_code_bank, _top_ram_bank, _top_window_invalid, _top_window_first
    rts

_test_snapshot_machine_state:
    lda _currentLog
    beq @snapshotA
    cmp #1
    beq @snapshotB
    rts
@snapshotA:
    SAVE_STATE _expected_a_start, _expected_a_end, _expected_a_pc, _expected_a_entry_ptr, _expected_a_file_ptr, _expected_a_code_bank, _expected_a_ram_bank, _expected_a_window_invalid, _expected_a_window_first
    rts
@snapshotB:
    SAVE_STATE _expected_b_start, _expected_b_end, _expected_b_pc, _expected_b_entry_ptr, _expected_b_file_ptr, _expected_b_code_bank, _expected_b_ram_bank, _expected_b_window_invalid, _expected_b_window_first
    rts

_test_capture_machine_state:
    lda _currentLog
    beq @captureA
    cmp #1
    beq @captureB
    rts
@captureA:
    SAVE_STATE _observed_a_start, _observed_a_end, _observed_a_pc, _observed_a_entry_ptr, _observed_a_file_ptr, _observed_a_code_bank, _observed_a_ram_bank, _observed_a_window_invalid, _observed_a_window_first
    rts
@captureB:
    SAVE_STATE _observed_b_start, _observed_b_end, _observed_b_pc, _observed_b_entry_ptr, _observed_b_file_ptr, _observed_b_code_bank, _observed_b_ram_bank, _observed_b_window_invalid, _observed_b_window_first
    rts

b1IsControllerSet:
    rts

.include "modulus.s"
.include "randomAsm.s"

.segment "BSS"
testPC: .res 2
