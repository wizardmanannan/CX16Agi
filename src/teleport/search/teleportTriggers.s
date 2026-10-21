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
; there. A walk that cannot see everything - more boxes or calls than kept,
; ifs nested too deep, a command it does not know, code cut short - says so,
; and the landing is reported as not fully checked.
;
; Ego's position read into variables (get.posn(ego, vx, vy), then lessn,
; greatern or equaln on them) gives a box too, but such tests also steer
; paths and priorities, so the box is kept only if its block does something
; - changes room, flags or inventory, prints, sounds or takes control.
; Either way those tests are "might be", not judged by stale values.
.importzp c_sp, ptr1, ptr3
.import subysp, addysp
.export _bFTeleportTriggers

TELEPORT_ZONES_MAX = (TELEPORT_ZONE_BYTES - 1) / 4
TELEPORT_CALLS_MAX = 4          ; called logics followed
TELEPORT_CODE_BUFFER = 64       ; logic code read at a time
TELEPORT_FRAMES = 16            ; nested ifs followed; deeper code ends the walk
TELEPORT_FRAME_SIZE = 6         ; end(2), else end(2), bits, position box
TELEPORT_TRIGGER_SCRATCH = TELEPORT_CODE_BUFFER + TELEPORT_FRAMES * TELEPORT_FRAME_SIZE
TELEPORT_ACTIONS = 182
TELEPORT_TESTS = 19
TELEPORT_GET_POSN = 39
TELEPORT_NO_BOX = $FF
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
bFBox: .res 4                   ; ego-position box of this condition: x1 y1 x2 y2
bFBoxState: .res 1              ; bit 0: narrowed; bit 7: not expressible
bFBoxIndex: .res 1              ; its index once added, else TELEPORT_NO_BOX
bFEgoKnown: .res 1              ; nonzero once get.posn(ego, ...) was seen
bFEgoX: .res 1                  ; the variables it read ego's X and Y into
bFEgoY: .res 1
bFGotoLow: .res 1
bFCalls: .res TELEPORT_CALLS_MAX
bFCallCount: .res 1
bFCallNext: .res 1
bFFrameTemp: .res 1
bFArgumentCount: .res 1
bFPushEnd: .res 2               ; else end being checked
bFPartial: .res 1               ; bit 7 set: something was not seen

; byte bFTeleportTriggers(byte* zones): zones[0] = count, then x1 y1 x2 y2
; per box. Returns nonzero if the walk could not see everything.
_bFTeleportTriggers:
    sta bFZones
    stx bFZones + 1
    stz bFZoneCount
    stz bFPartial
    stz bFEgoKnown
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
    lda bFPartial
    ldy #TELEPORT_TRIGGER_SCRATCH
    jmp addysp                  ; preserves A

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
    bcs @done                   ; the end of the code
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
    jsr bFTriggerAction
    bcc bFTriggerWalk
@end:
    jmp bFTriggerPartial
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
    bra @end
@done:
    rts

; Steps over action A and its arguments, noting what matters: get.posn on
; ego, and anything a position box should guard. Carry set at the end of
; the code.
bFTriggerAction:
    cmp #TELEPORT_GET_POSN
    beq @position
    pha
    jsr bFTriggerEffect
    bcc @skip
    jsr bFTriggerConfirm
@skip:
    pla
    jsr bFActionArgumentCount
    jmp bFTriggerSkip
@position:                      ; get.posn(object, vx, vy)
    jsr bFTriggerByte
    bcs @done
    tax
    jsr bFTriggerByte
    bcs @done
    sta bFConditionTemp
    jsr bFTriggerByte
    bcs @done
    cpx #0
    bne @other
    sta bFEgoY
    lda bFConditionTemp
    sta bFEgoX
    lda #1
    sta bFEgoKnown
@other:
    clc
@done:
    rts

; Carry set if action A does something a box should guard: flags (set ..
; toggle.v), new.room, inventory (get, get.v, drop), sound, print and
; display, program.control.
bFTriggerEffect:
    cmp #12
    bcc @no
    cmp #20
    bcc @yes
    cmp #92
    bcc @no
    cmp #95
    bcc @yes
    cmp #99
    beq @yes
    cmp #101
    bcc @no
    cmp #105
    bcc @yes
    cmp #131
    beq @yes
@no:
    clc
    rts
@yes:
    sec
    rts

; The enclosing blocks do something: their position boxes stay.
bFTriggerConfirm:
    ldx bFFrameDepth
    beq @done
    lda #TELEPORT_CODE_BUFFER + 5
@frame:
    tay
    lda #TELEPORT_NO_BOX
    sta (ptr3),y
    tya
    clc
    adc #TELEPORT_FRAME_SIZE
    dex
    bne @frame
@done:
    rts

; Remembers logic A to walk later: not logic 0 or the room, each once.
bFTriggerCall:
    tay
    beq @done
    cmp TELEPORT_VAR + 0
    beq @done
    ldx bFCallCount
@seen:
    dex
    bmi @new
    cmp bFCalls,x
    bne @seen
    rts
@new:
    ldx bFCallCount
    cpx #TELEPORT_CALLS_MAX
    bcs bFTriggerPartial
    sta bFCalls,x
    inc bFCallCount
@done:
    rts

; Something was left unseen.
bFTriggerPartial:
    sec
    ror bFPartial
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
    asl a
    sta bFFrameTemp
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
    iny                         ; a position box whose block did nothing
    lda (ptr3),y
    cmp #TELEPORT_NO_BOX
    beq @bits
    phy
    asl a                       ; x1 = y1 = 255: never hit, never cleared
    asl a
    tay
    iny
    lda #$FF
    sta (ptr1),y
    iny
    sta (ptr1),y
    ply
    sta (ptr3),y
@bits:
    dey
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
    lda #TELEPORT_NO_BOX
    sta bFBoxIndex
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
    iny
    lda bFBoxIndex
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
    stz bFBox                   ; the whole screen, until narrowed
    stz bFBox + 1
    lda #TELEPORT_WIDTH - 1
    sta bFBox + 2
    lda #TELEPORT_HEIGHT - 1
    sta bFBox + 3
    stz bFBoxState
    bra @test
@closed:                        ; a narrowed position box is added, pending
    ldx #TELEPORT_NO_BOX
    lda bFBoxState
    cmp #1
    bne @store
    ldx bFZoneCount
    phx
    ldx #bFBox - bFConditionArgs
    jsr bFTriggerAddBox
    plx
    bcc @store
    ldx #TELEPORT_NO_BOX
@store:
    stx bFBoxIndex
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
    ldx #1
    jsr bFTriggerAddBox
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
    ldx bFEgoKnown              ; a variable holding ego's position?
    beq @exact
    ldx bFConditionArgs
    cpx bFEgoX
    beq @onX
    cpx bFEgoY
    bne @exact
    ldx #1
    .byte $2C                   ; bit abs: skip the ldx
@onX:
    ldx #0
    jsr bFTriggerAxis
    bra @maybe
@exact:
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

; Adds the box at bFConditionArgs + X (x1 y1 x2 y2). Carry set if the list
; is full: then the walk is partial.
bFTriggerAddBox:
    lda bFZoneCount
    cmp #TELEPORT_ZONES_MAX
    bcc @room
    jsr bFTriggerPartial
    sec
    rts
@room:
    asl a
    asl a
    tay
    iny                         ; zones + 1 + 4 * count
@corner:
    lda bFConditionArgs,x
    sta (ptr1),y
    inx
    iny
    tya
    and #3
    cmp #1
    bne @corner                 ; four corners: up to the next 4k + 1
    inc bFZoneCount
    clc
    rts

; Narrows the condition's ego-position box on axis X (0: X, 1: Y) by test
; bFConditionTest against number bFConditionArgs + 1. Inside or(), against a
; variable, or a negated equality, the box cannot express it.
bFTriggerAxis:
    lda bFConditionInOr
    bne @unusable
    lda bFConditionTest
    lsr a                       ; 0 equaln, 1 lessn, 2 greatern; carry: number
    bcc @unusable
    ldy bFConditionArgs + 1
    cmp #1
    beq @less
    bcs @greater
    lda bFConditionNot          ; equaln: low = high = n
    bne @unusable
    tya
    jsr @raise
    tya
    bra @lower
@less:
    lda bFConditionNot          ; not lessn: x >= n
    beq @below
    tya
    bra @raise
@below:
    tya                         ; lessn: x <= n - 1
    beq @unusable
    dec a
    bra @lower
@greater:
    lda bFConditionNot          ; not greatern: x <= n
    beq @above
    tya
    bra @lower
@above:
    tya                         ; greatern: x >= n + 1
    inc a
    beq @unusable
@raise:                         ; low = max(low, A)
    cmp bFBox,x
    bcc @narrowed
    sta bFBox,x
    bra @narrowed
@lower:                         ; high = min(high, A)
    cmp bFBox + 2,x
    bcs @narrowed
    sta bFBox + 2,x
@narrowed:
    lda #1
    .byte $2C                   ; bit abs: skip the lda
@unusable:
    lda #$80
    tsb bFBoxState
    rts

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
