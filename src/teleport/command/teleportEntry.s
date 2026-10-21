.ifndef TELEPORT_ENTRY_INC
TELEPORT_ENTRY_INC = 1
; teleportEntry.s - How does a typed line reach the command bank?
;
; The parser's line is in bank 7, which bank 0xF cannot see. Copy it onto the
; C stack - no persistent buffer - and run the command there.
.importzp ptr1, c_sp
.import subysp, addysp
.export _b7TeleportCommand

TELEPORT_LINE_SIZE = 41         ; parser limit, terminator included

.segment "BANKRAM07"
; boolean b7TeleportCommand(const char* input): TRUE if it was a teleport command.
_b7TeleportCommand:
    sta ptr1
    stx ptr1 + 1
    jsr b7TeleportKeyword
    beq @done
    ldy #TELEPORT_LINE_SIZE
    jsr subysp
    ldy #$FF
@copy:
    iny
    lda (ptr1),y
    sta (c_sp),y
    bne @copy
    lda c_sp
    ldx c_sp + 1
    TRAMPOLINE #TELEPORT_BANK, _bFTeleportCommand
    ldy #TELEPORT_LINE_SIZE
    jsr addysp
    lda #1
@done:
    ldx #0
    rts
.endif
