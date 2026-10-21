.ifndef TELEPORT_COMMAND_INC
TELEPORT_COMMAND_INC = 1
; teleportCommand.s - What does a teleport command do?
;
; Runs a parsed command: cancel, reject with usage, find named rooms, show a
; tour's plan, then request the first stop. Any command ends a running tour.
; A 128-byte block on the C stack - directory entries, file reads, replies -
; is the only memory used, and only while the command runs.
.importzp c_sp, ptr1, ptr2, ptr3
.import subysp, addysp, _numLogics
.export _bFTeleportCommand

TELEPORT_SCRATCH_SIZE = 128

.segment "BANKRAM0F"
bFTeleportLine: .res 2
bFTeleportScratch: .res 2

TELEPORT_ASCII
; void bFTeleportCommand(char* line): line is the parser's line on the C stack.
_bFTeleportCommand:
    sta bFTeleportLine
    stx bFTeleportLine + 1
    stz _teleportTour
    ldy #TELEPORT_SCRATCH_SIZE
    jsr subysp
    lda c_sp
    sta bFTeleportScratch
    lda c_sp + 1
    sta bFTeleportScratch + 1
    jsr bFTeleportRun
    ldy #TELEPORT_SCRATCH_SIZE
    jmp addysp

bFTeleportRun:
    jsr bFTeleportLinePointer
    jsr bFTeleportGrammar
    bcs @usage
    lda _bFTeleportTourForm
    bne @rooms
    lda _bFTeleportWords        ; "teleport stop"
    beq @zero
    dec a
    bne @rooms
    lda _bFTeleportWordLength
    cmp #4
    bne @rooms
    ldy _bFTeleportWordStart
    ldx #0
@stop:
    lda (ptr1),y
    cmp bFStopWord,x
    bne @rooms
    iny
    inx
    cpx #4
    bne @stop
    bra @cancel
@zero:
    lda _bFTeleportStopRoom     ; "teleport 0 0 0"
    bne @rooms
    lda _bFTeleportStopX
    ora _bFTeleportStopY
    bne @usage
@cancel:
    stz _teleportPending
    lda #TELEPORT_CANCELED
    TRAMPOLINE #TELEPORT_NOTICE_BANK, _b7TeleportNotice
    rts

@rooms:
    ldx #0                      ; numbered stops must exist
@number:
    cpx _bFTeleportStopCount
    bcs @names
    lda _bFTeleportStopFirst,x
    cmp _bFTeleportStopFirst + 1,x
    bne @nextNumber
    lda _bFTeleportStopRoom,x
    beq @usage
    ldy _numLogics + 1
    bne @inGame
    cmp _numLogics
    bcs @usage
@inGame:
    phx
    ldx #0
    jsr bFTeleportDirectory
    plx
    bcs @usage
@nextNumber:
    inx
    bra @number
@usage:
    lda #<bFUsage
    ldx #>bFUsage
    sec
    jmp bFTeleportShow

@names:
    lda _bFTeleportWords
    beq @start
    lda #<bFSearching
    ldx #>bFSearching
    clc
    jsr bFTeleportShow
    jsr bFTeleportFindRooms
    TRAMPOLINE #TELEPORT_TEXT_BANK, _b3ClearLastPlacedText
    ldx #0
@found:
    cpx _bFTeleportStopCount
    bcs @start
    lda _bFTeleportStopFirst,x
    cmp _bFTeleportStopFirst + 1,x
    beq @nextFound
    lda _bFTeleportStopRoom,x
    beq @unknown
@nextFound:
    inx
    bra @found
@unknown:
    phx
    lda #<bFUnknown
    ldx #>bFUnknown
    jsr bFTextBegin
    plx
    jsr bFTextName
    jmp bFTextShow

@start:
    lda _bFTeleportStopCount
    dec a
    beq @go
    lda #<bFTour                ; show what each name became before leaving
    ldx #>bFTour
    jsr bFTextBegin
    lda _bFTeleportDelay
    jsr bFTextNumber
    lda #<bFTourSeconds
    ldx #>bFTourSeconds
    jsr bFTextString
    ldx #0
@plan:
    txa
    beq @first
    lda #','
    jsr bFTextChar
    lda #' '
    jsr bFTextChar
@first:
    lda _bFTeleportStopFirst,x
    cmp _bFTeleportStopFirst + 1,x
    beq @room
    jsr bFTextName
    lda #'='
    jsr bFTextChar
@room:
    phx
    lda _bFTeleportStopRoom,x
    jsr bFTextNumber
    plx
    inx
    cpx _bFTeleportStopCount
    bcc @plan
    jsr bFTextShow
@go:
    jmp bFTeleportTourBegin

; ptr1 = line. Preserves A, X and Y.
bFTeleportLinePointer:
    pha
    lda bFTeleportLine
    sta ptr1
    lda bFTeleportLine + 1
    sta ptr1 + 1
    pla
    rts

; ptr2 = line. Preserves A, X and Y.
bFTeleportLinePointer2:
    pha
    lda bFTeleportLine
    sta ptr2
    lda bFTeleportLine + 1
    sta ptr2 + 1
    pla
    rts

; ptr3 = scratch. Preserves A, X and Y.
bFTeleportScratchPointer:
    pha
    lda bFTeleportScratch
    sta ptr3
    lda bFTeleportScratch + 1
    sta ptr3 + 1
    pla
    rts

bFStopWord: .byte "stop"
bFUsage: .asciiz "Use: tp ROOM [X Y], tp NAME [X Y], tp:SECONDS:NAME,NAME or tp stop"
bFSearching: .asciiz "Teleport: searching rooms..."
bFUnknown: .asciiz "Teleport: no room matches "
bFTour: .asciiz "Teleport tour, "
bFTourSeconds: .asciiz " seconds per stop: "
.popcharmap
.endif
