; Assembly bridge for bounded C-owned recursive-dispatch snapshots.
.ifndef LOGIC_CONTEXT_INC
LOGIC_CONTEXT_INC = 1

.importzp ptr1, ptr2
.import _assmLogicEntry, _assmLogicFile, _currentLog
.struct ContextEntryLayout
    loaded .byte
    entryPoint .word
    currentPoint .word
    data .word
    dataBank .byte
    dependency .byte
.endstruct
.struct ContextFileLayout
    codeSize .word
    code .word
    messageCount .byte
    messages .word
    codeBank .byte
    messageBank .byte
.endstruct
; The C compile-time ABI check and linker adjacency assertion guard these
; field-derived lengths without retaining a runtime offset/size table.
__LOGIC_CONTEXT_ENTRY_BYTES__ = .sizeof(ContextEntryLayout)
__LOGIC_CONTEXT_METADATA_BYTES__ = .sizeof(ContextEntryLayout) + .sizeof(ContextFileLayout)
.global _logicContextCaptureCaller, _logicContextRestoreCaller

.segment "CODE"
contextCodeStart = *
; Save/restore use the same byte transfers. Only the two zero-page operands
; change direction. CODE is writable main RAM; IRQ code never calls this
; foreground-only routine, and nested logic calls finish copying first.
_logicContextCaptureCaller:
    sta ptr1
    stx ptr1 + 1
    ldy #0
    clc
    lda ZP_PTR_CODE
    adc cwCurrentCode
    sta (ptr1),y
    iny
    lda ZP_PTR_CODE + 1
    adc cwCurrentCode + 1
    sta (ptr1),y
    lda #<ptr2
    ldy #<ptr1
    bra contextCopy

_logicContextRestoreCaller:
    sta ptr1
    stx ptr1 + 1
    ldy #0
    lda (ptr1),y
    sta ZP_PTR_CODE
    iny
    lda (ptr1),y
    sta ZP_PTR_CODE + 1
    stz cwCurrentCode
    stz cwCurrentCode + 1
    lda #TRUE
    sta codeWindowInvalid
    lda #<ptr1
    ldy #<ptr2
contextCopy:
    sta contextCopyRead + 1
    sty contextCopyWrite + 1
    clc
    lda ptr1
    adc #2
    sta ptr1
    bcc :+
    inc ptr1 + 1
:
    ldx #0
contextCopyRegion:
    lda contextRegions,x
    sta ptr2
    inx
    lda contextRegions,x
    sta ptr2 + 1
    inx
    ldy contextRegions,x
    dey
contextCopyByte:
contextCopyRead:
    lda (ptr2),y
contextCopyWrite:
    sta (ptr1),y
    dey
    bpl contextCopyByte
    clc
    lda contextRegions,x
    adc ptr1
    sta ptr1
    bcc :+
    inc ptr1 + 1
:
    inx
    cpx #contextRegionsEnd - contextRegions
    bne contextCopyRegion
    rts

; Buffer order matches the checked C layout: PC, file/entry pointers, logic,
; RAM bank, metadata, and code bounds/bank. All lengths fit descending Y.
contextRegions:
    .word ZP_PTR_LF
    .byte 4
    .word _currentLog
    .byte 2
    .word RAM_BANK
    .byte 1
    .word _assmLogicEntry
    .byte __LOGIC_CONTEXT_METADATA_BYTES__
    .word startPos
    .byte 5
contextRegionsEnd:
    .assert ZP_PTR_LE = ZP_PTR_LF + 2, error, "context pointers must be contiguous"
    .assert _assmLogicFile = _assmLogicEntry + __LOGIC_CONTEXT_ENTRY_BYTES__, lderror, "logic metadata must be contiguous"
    .assert _codeBank = startPos + 4, lderror, "code bounds and bank must be contiguous"
    .assert __LOGIC_CONTEXT_METADATA_BYTES__ < 128, lderror, "context region exceeds descending-Y range"

.if !(.defined(LOGIC_CONTEXT_TRACE) .or .defined(LOGIC_CONTEXT_VERIFY))
; The release stack has eight 33-byte frames. Its byte-sized index avoids
; cc65's general multiplication and software-stack temporaries on each call.
contextPayloadBytes = 14 + __LOGIC_CONTEXT_METADATA_BYTES__
contextFrameBytes = contextPayloadBytes + 1
contextFrameCount = 8
.assert contextFrameBytes = 33, error, "update byte-index arithmetic for changed context layout"
.import _exitAllLogics
.global _logicContextEnter, _logicContextLeave, _logicContextAbandon
.global _logicContextDepthValue, _logicContextErrorValue

.segment "BSS"
contextFrames: .res contextFrameBytes * contextFrameCount
_logicContextDepthValue:
contextDepth: .res 1
_logicContextErrorValue:
contextError: .res 1

.segment "CODE"
_logicContextEnter:
    ldx contextDepth
    cpx #contextFrameCount
    bcc :+
    lda #1
    sta contextError
    bra contextReject
:
    jsr contextFrameAddress
    jsr _logicContextCaptureCaller
    ; The copier leaves ptr1 immediately after the saved payload.
    lda #0
    sta (ptr1)
    sta contextError
    inc contextDepth
    bra contextAccepted

_logicContextLeave:
    ldx contextDepth
    bne :+
    lda #2
    sta contextError
contextReject:
    lda #1
    sta _exitAllLogics
    ldx #0
    txa
    rts
:
    dex
    jsr contextFrameAddress
    ldy #contextPayloadBytes
    lda (ptr1),y
    bne contextAbandoned
    lda ptr1
    jsr _logicContextRestoreCaller
    bra contextPopped
contextAbandoned:
    stz cwCurrentCode
    stz cwCurrentCode + 1
    lda #TRUE
    sta codeWindowInvalid
contextPopped:
    dec contextDepth
contextAccepted:
    ldx #0
    lda #1
    rts

_logicContextAbandon:
    ldx contextDepth
    beq :+
    dex
    jsr contextFrameAddress
    ldy #contextPayloadBytes
    lda #TRUE
    sta (ptr1),y
:
    rts

; X is 0..7: 33*X fits in one byte and leaves carry clear.
; Return both AX and ptr1; preserve X as the pointer's high byte.
contextFrameAddress:
    stx ptr2
    txa
    asl
    asl
    asl
    asl
    asl
    adc ptr2
    adc #<contextFrames
    sta ptr1
    lda #>contextFrames
    adc #0
    sta ptr1 + 1
    tax
    lda ptr1
    rts

.endif

.if .defined(LOGIC_CONTEXT_TRACE) .or .defined(LOGIC_CONTEXT_VERIFY)
.importzp c_sp
.import _logicContextHardwareSp
.import _logicContextSoftwareSp

.global _logicContextCaptureStackPointers

.segment "CODE"
_logicContextCaptureStackPointers:
    tsx
    stx _logicContextHardwareSp
    lda c_sp
    sta _logicContextSoftwareSp
    lda c_sp + 1
    sta _logicContextSoftwareSp + 1
    rts
.endif

_logicContextCodeBytes = * - contextCodeStart
.export _logicContextCodeBytes

.endif
