.ifndef TELEPORT_TRIGGERS_INC
TELEPORT_TRIGGERS_INC = 1
; teleportTriggers.s - Which of this room's position triggers could fire now?
;
; A room script starts scenes, speech and deaths with tests such as
; posn(ego, x1, y1, x2, y2) in its conditions. This reads the current room's
; logic code from its VOL file and walks it without running it, evaluating
; each condition against the current variables and flags: var and flag tests
; are exact; said, have.key and controller need the player, so are false;
; anything else is "might be". A box on ego counts when its condition, and
; every enclosing if or else, might be true. Logics the room calls (call,
; call.v; up to four) are walked the same way afterwards. Teleport then lands
; outside these boxes, so arriving starts nothing - like loading a saved game
; there. Positions compared through variables (get.posn, then lessn...) are
; left out: in practice they mostly steer priorities and paths, and counting
; them would fence off whole rooms.
.importzp c_sp, ptr1, ptr3
.import subysp, addysp
.export _bFTeleportTriggers

TELEPORT_ZONES_MAX = 32         ; boxes kept; more are ignored
TELEPORT_CALLS_MAX = 4          ; called logics followed
TELEPORT_CODE_BUFFER = 64       ; logic code read at a time
TELEPORT_FRAMES = 16            ; nested ifs followed; deeper code ends the walk
TELEPORT_FRAME_SIZE = 5         ; end(2), else end(2), bits
TELEPORT_TRIGGER_SCRATCH = TELEPORT_CODE_BUFFER + TELEPORT_FRAMES * TELEPORT_FRAME_SIZE
TELEPORT_ACTIONS = 182
TELEPORT_TESTS = 19
CAN_TRUE = 1                    ; condition values: which outcomes are possible
CAN_FALSE = 2
FRAME_ACTIVE = 1                ; frame bits
FRAME_CAN_FALSE = 2
FRAME_ELSE = 4
FRAME_ENCLOSING = 8

.segment "BANKRAM0F"
bFZones: .res 2
bFZoneCount: .res 1
bFZoneStart: .res 1
bFCodeIndex: .res 1
bFCodeFill: .res 1
bFCodePosition: .res 2
bFFrameDepth: .res 1
bFCondition: .res 1
bFConditionOr: .res 1
bFConditionInOr: .res 1
bFConditionNot: .res 1
bFConditionTest: .res 1
bFConditionTemp: .res 1
bFConditionArgs: .res 5
bFGotoLow: .res 1
bFCalls: .res TELEPORT_CALLS_MAX
bFCallCount: .res 1
bFCallNext: .res 1
bFFrameTemp: .res 1
bFArgumentCount: .res 1
bFPushEnd: .res 2               ; else end being checked

; void bFTeleportTriggers(byte* zones): zones[0] = count, then x1 y1 x2 y2 per box.
_bFTeleportTriggers:
    sta bFZones
    stx bFZones + 1
    stz bFZoneCount
    ldy #TELEPORT_TRIGGER_SCRATCH
    jsr subysp
    lda c_sp
    sta bFTeleportScratch
    lda c_sp + 1
    sta bFTeleportScratch + 1
    stz bFCallCount
    stz bFCallNext
    lda TELEPORT_VAR + 0
@logic:
    jsr bFTriggerLogic
    ldx bFCallNext
    cpx bFCallCount
    bcs @written
    inc bFCallNext
    lda bFCalls,x
    bra @logic
@written:
    lda bFZones
    sta ptr1
    lda bFZones + 1
    sta ptr1 + 1
    lda bFZoneCount
    sta (ptr1)
    ldy #TELEPORT_TRIGGER_SCRATCH
    jmp addysp

; Reads logic A's header, then walks its code.
bFTriggerLogic:
    ldx #0
    jsr bFTeleportDirectory
    bcs @done
    ldy #3
@position:
    lda (ptr3),y
    sta bFSearchOffset - 1,y
    dey
    bne @position
    lda (ptr3)
    jsr bFSearchOpen
    lda #7                      ; 12 34 volume size(2) code-size(2)
    jsr bFSearchReadAt
    bne @close
    lda (ptr3)
    cmp #$12
    bne @close
    ldy #5
    lda (ptr3),y
    sta bFSearchLeft
    iny
    lda (ptr3),y
    sta bFSearchLeft + 1
    stz bFCodeIndex
    stz bFCodeFill
    stz bFCodePosition
    stz bFCodePosition + 1
    stz bFFrameDepth
    lda bFZones
    sta ptr1
    lda bFZones + 1
    sta ptr1 + 1
    jsr bFTriggerWalk
@close:
    lda #TELEPORT_LFN
    jmp TELEPORT_CLOSE
@done:
    rts

; The next code byte in A. Carry set at the end of the code. Preserves X.
bFTriggerByte:
    ldy bFCodeIndex
    cpy bFCodeFill
    bcc @have
    lda bFSearchLeft + 1
    bne @full
    lda bFSearchLeft
    beq @end
    cmp #TELEPORT_CODE_BUFFER
    bcc @read
@full:
    lda #TELEPORT_CODE_BUFFER
@read:
    phx
    jsr bFSearchRead
    plx
    cmp #0                      ; bytes not read
    bne @end
    lda bFSearchCount
    sta bFCodeFill
    lda bFSearchLeft
    sec
    sbc bFCodeFill
    sta bFSearchLeft
    bcs @counted
    dec bFSearchLeft + 1
@counted:
    ldy #0
@have:
    lda (ptr3),y
    iny
    sty bFCodeIndex
    inc bFCodePosition
    bne @done
    inc bFCodePosition + 1
@done:
    clc
    rts
@end:
    sec
    rts

; Skips A code bytes. Carry set at the end of the code.
bFTriggerSkip:
    tax
    beq @done
@byte:
    jsr bFTriggerByte
    bcs @end
    dex
    bne @byte
@done:
    clc
@end:
    rts

; A = argument bytes of action command A (0-181).
bFActionArgumentCount:
    lsr a
    tax
    lda bFActionArguments,x
    bcs @low
    lsr a
    lsr a
    lsr a
    lsr a
    rts
@low:
    and #$0F
    rts

; A = argument bytes of test command A (0-18): its table follows the actions'.
bFTestArgumentCount:
    clc
    adc #TELEPORT_ACTIONS
    bra bFActionArgumentCount

; Walks statements to the end of the code.
bFTriggerWalk:
    jsr bFTriggerUnwind
    jsr bFTriggerByte
    bcs @end
    cmp #$FF
    beq @if
    cmp #$FE
    beq @goto
    cmp #TELEPORT_ACTIONS
    bcs @end
    cmp #$16                    ; call(n)
    beq @call
    cmp #$17                    ; call.v(v)
    beq @callVariable
    jsr bFActionArgumentCount
    jsr bFTriggerSkip
    bcc bFTriggerWalk
@end:
    rts
@call:
    jsr bFTriggerByte
    bcs @end
    jsr bFTriggerCall
    bra bFTriggerWalk
@callVariable:
    jsr bFTriggerByte
    bcs @end
    tax
    lda TELEPORT_VAR,x
    jsr bFTriggerCall
    bra bFTriggerWalk
@goto:
    jsr bFTriggerByte
    bcs @end
    sta bFGotoLow
    jsr bFTriggerByte
    bcs @end
    jsr bFTriggerElse
    bra bFTriggerWalk
@if:
    jsr bFTriggerCondition
    bcs @end
    jsr bFTriggerPush
    bcc bFTriggerWalk
    rts

; Remembers logic A to walk later: not logic 0 or the room, each once.
bFTriggerCall:
    tay
    beq @done
    cmp TELEPORT_VAR + 0
    beq @done
    ldx bFCallCount
    cpx #TELEPORT_CALLS_MAX
    bcs @done
@seen:
    dex
    bmi @new
    cmp bFCalls,x
    bne @seen
    rts
@new:
    ldx bFCallCount
    sta bFCalls,x
    inc bFCallCount
@done:
    rts

; A goto that ends an if block and jumps forward, but not past the end of
; the enclosing block, begins its else block. One that leaves the enclosing
; block is an early exit, not an else. In: A = offset high byte, bFGotoLow.
bFTriggerElse:
    cmp #$80
    bcs @done                   ; backward: a loop, not an else
    ldx bFFrameDepth
    beq @done
    pha
    jsr bFTriggerFrame          ; Y = top frame
    lda bFCodePosition
    cmp (ptr3),y
    bne @pull
    iny
    lda bFCodePosition + 1
    cmp (ptr3),y
    bne @pull
    lda bFCodePosition          ; else end = here + offset
    clc
    adc bFGotoLow
    sta bFPushEnd
    pla
    adc bFCodePosition + 1
    sta bFPushEnd + 1
    cpx #2
    bcc @else
    tya                         ; the enclosing block's end
    sec
    sbc #TELEPORT_FRAME_SIZE + 1
    tay
    lda (ptr3),y
    cmp bFPushEnd
    iny
    lda (ptr3),y
    sbc bFPushEnd + 1
    bcc @done                   ; it ends first: an early exit
@else:
    jsr bFTriggerFrame
    iny
    iny
    lda bFPushEnd
    sta (ptr3),y
    iny
    lda bFPushEnd + 1
    sta (ptr3),y
    iny
    lda (ptr3),y
    ora #FRAME_ELSE
    sta (ptr3),y
@done:
    rts
@pull:
    pla
    rts

; Y = offset of the top frame in scratch.
bFTriggerFrame:
    lda bFFrameDepth
    dec a
    sta bFFrameTemp
    asl a
    asl a
    adc bFFrameTemp
    adc #TELEPORT_CODE_BUFFER
    tay
    rts

; A = FRAME_ACTIVE if code here can run (every enclosing block might).
bFTriggerActive:
    lda bFFrameDepth
    beq @top
    jsr bFTriggerFrame
    iny
    iny
    iny
    iny
    lda (ptr3),y
    and #FRAME_ACTIVE
    rts
@top:
    lda #FRAME_ACTIVE
    rts

; Leaves every block that ends here; an if with an else continues as the else.
bFTriggerUnwind:
    lda bFFrameDepth
    beq @done
    jsr bFTriggerFrame
    lda bFCodePosition
    cmp (ptr3),y
    iny
    lda bFCodePosition + 1
    sbc (ptr3),y
    bcc @done                   ; still inside
    iny
    iny
    iny
    lda (ptr3),y                ; bits
    and #FRAME_ELSE
    beq @pop
    lda (ptr3),y                ; else: active if the parent is and the if
    and #FRAME_ENCLOSING        ; condition might be false
    beq @inactive
    lda (ptr3),y
    and #FRAME_CAN_FALSE
    beq @inactive
    lda #FRAME_ACTIVE | FRAME_ENCLOSING
    .byte $2C                   ; bit abs: skip the lda
@inactive:
    lda #0
    sta (ptr3),y
    dey                         ; end = else end
    lda (ptr3),y
    dey
    dey
    sta (ptr3),y
    iny
    lda (ptr3),y
    dey
    dey
    sta (ptr3),y
    bra bFTriggerUnwind
@pop:
    dec bFFrameDepth
    bra bFTriggerUnwind
@done:
    rts

; Pushes the if block whose condition is in bFCondition. Carry set if too deep.
bFTriggerPush:
    jsr bFTriggerByte           ; block size
    bcs @done
    sta bFGotoLow
    jsr bFTriggerByte
    bcs @done
    pha
    jsr bFTriggerActive
    asl a
    asl a
    asl a                       ; FRAME_ENCLOSING
    sta bFConditionTemp
    lsr a
    lsr a
    lsr a
    and bFCondition             ; active: enclosing and might be true
    bne @active
    lda bFZoneStart             ; its boxes cannot fire
    sta bFZoneCount
    lda #0
@active:
    ora bFConditionTemp
    sta bFConditionTemp
    lda bFCondition
    and #CAN_FALSE
    ora bFConditionTemp
    sta bFConditionTemp
    lda bFFrameDepth
    cmp #TELEPORT_FRAMES
    bcs @deep
    inc bFFrameDepth
    jsr bFTriggerFrame
    lda bFCodePosition          ; end = here + size
    clc
    adc bFGotoLow
    sta (ptr3),y
    iny
    pla
    adc bFCodePosition + 1
    sta (ptr3),y
    iny
    iny
    iny
    lda bFConditionTemp
    sta (ptr3),y
    clc
@done:
    rts
@deep:
    pla
    sec
    rts

; Reads a condition up to its closing $FF into bFCondition (CAN_* bits),
; adding its boxes on ego. Carry set at the end of the code or on an
; unknown test.
bFTriggerCondition:
    lda bFZoneCount
    sta bFZoneStart
    lda #CAN_TRUE
    sta bFCondition
    stz bFConditionInOr
    stz bFConditionNot
    bra @test
@closed:
    clc
    rts
@failed:
    sec
    rts
@test:
    jsr bFTriggerByte
    bcs @failed
    cmp #$FF
    beq @closed
    cmp #$FC                    ; or( ... )
    bne @notOr
    lda bFConditionInOr
    eor #1
    sta bFConditionInOr
    beq @closeOr
    lda #CAN_FALSE
    sta bFConditionOr
    bra @test
@closeOr:
    lda bFConditionOr
    jsr bFTriggerAnd
    bra @test
@notOr:
    cmp #$FD                    ; not
    bne @notNot
    sta bFConditionNot
    bra @test
@notNot:
    cmp #$0E                    ; said(count, words...)
    bne @fixed
    jsr bFTriggerByte
    bcs @failed
    asl a
    jsr bFTriggerSkip
    bcs @failed
    lda #CAN_FALSE
    bra @value
@fixed:
    cmp #TELEPORT_TESTS
    bcs @failed
    sta bFConditionTest
    jsr bFTestArgumentCount
    sta bFArgumentCount
    beq @evaluate
    ldx #0
@argument:
    jsr bFTriggerByte           ; preserves X
    bcs @failed
    sta bFConditionArgs,x
    inx
    cpx bFArgumentCount
    bcc @argument
@evaluate:
    jsr bFTriggerEvaluate
@value:
    ldx bFConditionNot
    beq @combine
    stz bFConditionNot
    cmp #CAN_TRUE | CAN_FALSE
    beq @combine
    eor #CAN_TRUE | CAN_FALSE
@combine:
    ldx bFConditionInOr
    bne @or
    jsr bFTriggerAnd
    bra @test
@or:
    tax                         ; or: might be true if either might,
    ora bFConditionOr           ; false only if both might be
    and #CAN_TRUE
    sta bFConditionTemp
    txa
    and bFConditionOr
    and #CAN_FALSE
    ora bFConditionTemp
    sta bFConditionOr
    jmp @test

; bFCondition = bFCondition and A.
bFTriggerAnd:
    tax
    and bFCondition
    and #CAN_TRUE
    sta bFConditionTemp
    txa
    ora bFCondition
    and #CAN_FALSE
    ora bFConditionTemp
    sta bFCondition
    rts

; A = CAN_* outcomes of test bFConditionTest with bFConditionArgs.
bFTriggerEvaluate:
    lda bFConditionTest
    cmp #7
    bcc @compare                ; 1-6: equal/less/greater, n or v
    beq @isset
    cmp #8
    beq @issetv
    cmp #$0B
    beq @position
    cmp #$0C
    bcc @maybe                  ; has, obj.in.room
    cmp #$0F
    bcc @false                  ; controller, have.key: the player's doing
    cmp #$10
    bcs @position               ; obj.in.box, center.posn, right.posn
@maybe:
    lda #CAN_TRUE | CAN_FALSE
    rts
@position:
    lda bFConditionArgs         ; on ego: a box to avoid. Its own test is
    bne @maybe                  ; taken as true; under not it is unknown.
    lda bFConditionNot
    bne @maybe
    ldx bFZoneCount
    cpx #TELEPORT_ZONES_MAX
    bcs @true
    txa
    asl a
    asl a
    tay
    iny                         ; zones + 1 + 4 * count
    ldx #1
@corner:
    lda bFConditionArgs,x
    sta (ptr1),y
    iny
    inx
    cpx #5
    bne @corner
    inc bFZoneCount
@true:
    lda #CAN_TRUE
    rts
@isset:
    ldx bFConditionArgs
    lda TELEPORT_FLAG,x
    bra @bool
@issetv:
    ldx bFConditionArgs
    lda TELEPORT_VAR,x
    tax
    lda TELEPORT_FLAG,x
@bool:
    bne @true
@false:
    lda #CAN_FALSE
    rts
@compare:
    lsr a                       ; odd: number form
    lda bFConditionArgs + 1
    bcs @right
    tax
    lda TELEPORT_VAR,x
@right:
    sta bFConditionTemp
    ldx bFConditionArgs
    lda TELEPORT_VAR,x
    cmp bFConditionTemp
    php
    lda bFConditionTest
    cmp #3
    bcc @equal
    cmp #5
    bcc @less
    plp                         ; greater
    beq @false
    bcs @true
    bra @false
@equal:
    plp
    beq @true
    bra @false
@less:
    plp
    bcc @true
    bra @false

; Argument bytes per command, two per byte (even command in the high nibble).
; Action commands 0-181 (AGI 2), from AGI Studio's command table.
bFActionArguments:
    .byte $01, $12, $22, $22, $22, $22, $11, $11, $11, $11, $11, $11, $11, $01, $10, $11
    .byte $11, $01, $13, $33, $32, $22, $21, $12, $22, $22, $22, $22, $12, $11, $11, $11
    .byte $11, $11, $13, $11, $12, $12, $21, $12, $25, $53, $11, $22, $11, $40, $11, $12
    .byte $22, $12, $01, $13, $33, $00, $12, $13, $00, $25, $21, $20, $03, $77, $00, $00
    .byte $01, $30, $01, $10, $00, $00, $00, $11, $10, $03, $30, $34, $41, $52, $12, $01
    .byte $10, $10, $02, $22, $20, $10, $00, $11, $01, $04, $20
; Test commands 0-18; said (14) is variable and handled apart.
bFTestArguments:
    .byte $02, $22, $22, $21, $11, $25, $10, $02, $55, $50
.endif
