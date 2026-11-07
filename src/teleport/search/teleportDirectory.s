.ifndef TELEPORT_DIRECTORY_INC
TELEPORT_DIRECTORY_INC = 1
; teleportDirectory.s - Where is a room's logic or picture on disk?
;
; Copies one 5-byte AGIFilePosType (volume, 32-bit offset) from LOGDIR or
; PICDIR in bank 0x10 into the command's scratch block.
.importzp ptr3, tmp1, tmp2
.import pusha, pushax, _memCpyBanked, _logdir, _picdir

TELEPORT_ENTRY_SIZE = 5

.segment "BANKRAM0F"
bFDirectoryLow: .lobytes _logdir, _picdir
bFDirectoryHigh: .hibytes _logdir, _picdir

; In: A = room, X = 0 for LOGDIR or 1 for PICDIR.
; Out: scratch[0..4] = entry, ptr3 = scratch, carry set if the entry is empty.
bFTeleportDirectory:
    sta tmp1                    ; room * 5 + directory
    stz tmp2
    asl a
    rol tmp2
    asl a
    rol tmp2
    adc tmp1                    ; carry is clear: tmp2 took the high bits
    bcc @noCarry
    inc tmp2
    clc
@noCarry:
    adc bFDirectoryLow,x
    sta tmp1
    lda tmp2
    adc bFDirectoryHigh,x
    sta tmp2
    lda bFTeleportScratch
    ldx bFTeleportScratch + 1
    jsr pushax
    lda tmp1
    ldx tmp2
    jsr pushax
    lda #TELEPORT_DIRECTORY_BANK
    jsr pusha
    lda #TELEPORT_ENTRY_SIZE
    ldx #0
    jsr _memCpyBanked
    jsr bFTeleportScratchPointer
    ldy #1                      ; EMPTY is offset $0FFFFF
    lda (ptr3),y
    iny
    and (ptr3),y
    cmp #$FF
    bne @present
    iny
    lda (ptr3),y
    cmp #$0F
    beq @empty                  ; carry is set
@present:
    clc
@empty:
    rts
.endif
