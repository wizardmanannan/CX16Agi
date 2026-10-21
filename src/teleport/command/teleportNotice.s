.ifndef TELEPORT_NOTICE_INC
TELEPORT_NOTICE_INC = 1
; teleportNotice.s - How is a teleport message shown?
;
; One copy of the message-box sequence, beside the notice texts in bank 7.
; Bank 0xF borrows it for its own texts.
.importzp c_sp, tmp1, tmp2
.import pusha, pushax, incsp2
.import _b3DisplayMessageBox, _b3ClearLastPlacedText, _b5WaitOnSpecificKeys
.export _b7TeleportNotice

TELEPORT_KEY_ENTER = $0D
TELEPORT_KEY_ESC = $1B
TELEPORT_BOX_AUTO = 255         ; AUTO_CALC_ROW / AUTO_CALC_COLUMN
TELEPORT_BOX_PALETTE = 1        ; TEXTBOX_PALETTE_NUMBER
TELEPORT_BOX_WIDTH = 32         ; DEFAULT_BOX_WIDTH
TELEPORT_BOX_FIRST_ROW = 4      ; FIRST_TEXT_ROW

.segment "BANKRAM07"
TELEPORT_ASCII

; Shows the message at A/X, stored in bank tmp1 (any value for low RAM).
; If tmp2 is nonzero, waits for Enter or Escape, then removes the box.
b7TeleportShow:
    ldy tmp2
    phy
    jsr pushax
    lda tmp1
    jsr pusha
    lda #TELEPORT_BOX_AUTO
    jsr pusha                   ; row
    jsr pusha                   ; column
    lda #TELEPORT_BOX_PALETTE
    jsr pusha
    lda #TELEPORT_BOX_WIDTH
    jsr pusha
    lda #1                      ; wrap
    jsr pusha
    lda #TELEPORT_BOX_FIRST_ROW
    TRAMPOLINE #TELEPORT_TEXT_BANK, _b3DisplayMessageBox
    pla
    beq @done
    lda #TELEPORT_KEY_ESC
    jsr pusha
    lda #TELEPORT_KEY_ENTER
    jsr pusha                   ; sp -> Enter, Escape
    lda c_sp
    ldx c_sp + 1
    jsr pushax
    lda #2
    TRAMPOLINE #TELEPORT_KEYS_BANK, _b5WaitOnSpecificKeys
    jsr incsp2
    TRAMPOLINE #TELEPORT_TEXT_BANK, _b3ClearLastPlacedText
@done:
    rts

; void b7TeleportNotice(byte reason): TELEPORT_BLOCKED..TELEPORT_UNCHECKED
_b7TeleportNotice:
    tay
    lda #TELEPORT_NOTICE_BANK
    sta tmp1
    sta tmp2
    lda b7NoticeLow - 1,y
    ldx b7NoticeHigh - 1,y
    bra b7TeleportShow

b7NoticeLow: .lobytes b7Blocked, b7WrongRoom, b7Canceled, b7Nearest, b7Exposed, b7Unchecked
b7NoticeHigh: .hibytes b7Blocked, b7WrongRoom, b7Canceled, b7Nearest, b7Exposed, b7Unchecked
b7Blocked: .asciiz "Teleport rejected: blocked path or scripted movement."
b7WrongRoom: .asciiz "Teleport canceled: entered a different room."
b7Canceled: .asciiz "Teleport canceled."
b7Nearest: .asciiz "Placed at nearest reachable point."
b7Exposed: .asciiz "No quiet spot here: placed where the room may react."
b7Unchecked: .asciiz "Room too complex to check fully: it may react."
.popcharmap
.endif
