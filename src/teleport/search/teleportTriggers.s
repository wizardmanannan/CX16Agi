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
; distance(ego, object, v) followed by lessn, equaln or not greatern on v
; gives, the same way, the box around the object's position (|dx| + |dy|
; <= r inside x +- r, y +- r). Before entry objects still stand where the
; old room left them, so then such tests are only "might be".
.importzp c_sp, ptr1, ptr3
.import subysp, addysp
.export _b1TeleportTriggers

TELEPORT_ZONES_MAX = (TELEPORT_ZONE_BYTES - 1) / 4
TELEPORT_CALLS_MAX = 4          ; called logics followed
TELEPORT_FRAMES = 16            ; nested ifs followed; deeper code ends the walk
TELEPORT_FRAME_SIZE = 6         ; end(2), else end(2), bits, position box
TELEPORT_TRIGGER_SCRATCH = TELEPORT_CODE_BUFFER + TELEPORT_FRAMES * TELEPORT_FRAME_SIZE
TELEPORT_ACTIONS = 182
TELEPORT_TESTS = 19
TELEPORT_GET_POSN = 39
TELEPORT_DISTANCE = 69
TELEPORT_NEAR_MAX = 4           ; distance() variables followed
TELEPORT_NO_BOX = $FF
CAN_TRUE = 1                    ; condition values: which outcomes are possible
CAN_FALSE = 2
FRAME_ACTIVE = 1                ; frame bits
FRAME_CAN_FALSE = 2
FRAME_ELSE = 4
FRAME_ENCLOSING = 8

.segment "BANKRAM01"
b1Zones: .res 2
b1TriggerBuffer: .res 2         ; code buffer, then frames, on the C stack
b1ZoneCount: .res 1
b1ZoneStart: .res 1
b1CodeIndex: .res 1
b1CodeFill: .res 1
b1CodePosition: .res 2
b1FrameDepth: .res 1
b1Condition: .res 1
b1ConditionOr: .res 1
b1ConditionInOr: .res 1
b1ConditionNot: .res 1
b1ConditionTest: .res 1
b1ConditionTemp: .res 1
b1ConditionArgs: .res 5
b1Box: .res 4                   ; ego-position box of this condition: x1 y1 x2 y2
b1BoxState: .res 1              ; bit 0: narrowed; bit 7: not expressible
b1BoxIndex: .res 1              ; its index once added, else TELEPORT_NO_BOX
b1EgoKnown: .res 1              ; nonzero once get.posn(ego, ...) was seen
b1EgoX: .res 1                  ; the variables it read ego's X and Y into
b1EgoY: .res 1
b1NearCount: .res 1             ; distance(ego, object, v) results followed
b1NearVar: .res TELEPORT_NEAR_MAX
b1NearX: .res TELEPORT_NEAR_MAX ; the object's position, or
b1NearY: .res TELEPORT_NEAR_MAX ; ... X = Y = $FF if unknown or hidden
b1NearRadius: .res 1
b1GotoLow: .res 1
b1Calls: .res TELEPORT_CALLS_MAX
b1CallCount: .res 1
b1CallNext: .res 1
b1FrameTemp: .res 1
b1ArgumentCount: .res 1
b1PushEnd: .res 2               ; else end being checked
b1Partial: .res 1               ; bit 7 set: something was not seen

; byte b1TeleportTriggers(byte* zones): zones[0] = count, then x1 y1 x2 y2
; per box. Returns nonzero if the walk could not see everything.
_b1TeleportTriggers:
    sta b1Zones
    stx b1Zones + 1
    stz b1ZoneCount
    stz b1Partial
    stz b1EgoKnown
    stz b1NearCount
    ldy #TELEPORT_TRIGGER_SCRATCH
    jsr subysp
    lda c_sp
    sta b1TriggerBuffer
    lda c_sp + 1
    sta b1TriggerBuffer + 1
    stz b1CallCount
    stz b1CallNext
    lda TELEPORT_VAR + 0
@logic:
    jsr b1TriggerLogic
    ldx b1CallNext
    cpx b1CallCount
    bcs @written
    inc b1CallNext
    lda b1Calls,x
    bra @logic
@written:
    lda b1Zones
    sta ptr1
    lda b1Zones + 1
    sta ptr1 + 1
    lda b1ZoneCount
    sta (ptr1)
    lda b1Partial
    ldy #TELEPORT_TRIGGER_SCRATCH
    jmp addysp                  ; preserves A

; Walks logic A's code, read through bank 0xF's file access.
b1TriggerLogic:
    ldx b1TriggerBuffer         ; reads go to the code buffer
    stx ptr3
    ldx b1TriggerBuffer + 1
    stx ptr3 + 1
    TRAMPOLINE #TELEPORT_BANK, bFTeleportOpenCode
    bcs @done
    stz b1CodeIndex
    stz b1CodeFill
    stz b1CodePosition
    stz b1CodePosition + 1
    stz b1FrameDepth
    lda b1Zones
    sta ptr1
    lda b1Zones + 1
    sta ptr1 + 1
    jsr b1TriggerWalk
    lda #TELEPORT_LFN
    jmp TELEPORT_CLOSE
@done:
    rts

; The next code byte in A. Carry set at the end of the code. Preserves X.
b1TriggerByte:
    ldy b1CodeIndex
    cpy b1CodeFill
    bcc @have
    phx
    lda #TELEPORT_CODE_BUFFER
    TRAMPOLINE #TELEPORT_BANK, bFTeleportReadCode
    plx
    sta b1CodeFill
    cmp #1
    bcc @end                    ; nothing more
    ldy #0
@have:
    lda (ptr3),y
    iny
    sty b1CodeIndex
    inc b1CodePosition
    bne @done
    inc b1CodePosition + 1
@done:
    clc
    rts
@end:
    sec
    rts

; Skips A code bytes. Carry set at the end of the code.
b1TriggerSkip:
    tax
    beq @done
@byte:
    jsr b1TriggerByte
    bcs @end
    dex
    bne @byte
@done:
    clc
@end:
    rts

; A = argument bytes of action command A (0-181).
b1ActionArgumentCount:
    lsr a
    tax
    lda b1ActionArguments,x
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
b1TestArgumentCount:
    clc
    adc #TELEPORT_ACTIONS
    bra b1ActionArgumentCount

; Walks statements to the end of the code.
b1TriggerWalk:
    jsr b1TriggerUnwind
    jsr b1TriggerByte
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
    jsr b1TriggerAction
    bcc b1TriggerWalk
@end:
    jmp b1TriggerPartial
@call:
    jsr b1TriggerByte
    bcs @end
    jsr b1TriggerCall
    bra b1TriggerWalk
@callVariable:
    jsr b1TriggerByte
    bcs @end
    tax
    lda TELEPORT_VAR,x
    jsr b1TriggerCall
    bra b1TriggerWalk
@goto:
    jsr b1TriggerByte
    bcs @end
    sta b1GotoLow
    jsr b1TriggerByte
    bcs @end
    jsr b1TriggerElse
    bra b1TriggerWalk
@if:
    jsr b1TriggerCondition
    bcs @end
    jsr b1TriggerPush
    bcc b1TriggerWalk
    bra @end
@done:
    rts

; Steps over action A and its arguments, noting what matters: get.posn on
; ego, and anything a position box should guard. Carry set at the end of
; the code.
b1TriggerAction:
    cmp #TELEPORT_GET_POSN
    beq @position
    cmp #TELEPORT_DISTANCE
    beq b1TriggerDistance
    pha
    jsr b1TriggerEffect
    bcc @skip
    jsr b1TriggerConfirm
@skip:
    pla
    jsr b1ActionArgumentCount
    jmp b1TriggerSkip
@position:                      ; get.posn(object, vx, vy)
    jsr b1TriggerByte
    bcs @done
    tax
    jsr b1TriggerByte
    bcs @done
    sta b1ConditionTemp
    jsr b1TriggerByte
    bcs @done
    cpx #0
    bne @other
    sta b1EgoY
    lda b1ConditionTemp
    sta b1EgoX
    lda #1
    sta b1EgoKnown
@other:
    clc
@done:
    rts

; distance(a, b, v): remembers v if one of a, b is ego, with where the other
; stands. Carry set at the end of the code.
b1TriggerDistance:
    jsr b1TriggerByte
    bcs @done
    sta b1ConditionArgs
    jsr b1TriggerByte
    bcs @done
    sta b1ConditionArgs + 1
    jsr b1TriggerByte
    bcs @done
    sta b1ConditionArgs + 2
    ldx b1ConditionArgs + 1     ; b
    lda b1ConditionArgs         ; a
    beq @ego                    ; a is ego: the other is b
    cpx #0
    bne @other                  ; neither is ego
    tax                         ; b is ego: the other is a
@ego:
    txa
    beq @other                  ; both are ego
    ldy b1NearCount             ; the same variable again: its entry
@find:
    dey
    bmi @new
    lda b1NearVar,y
    cmp b1ConditionArgs + 2
    bne @find
    bra @entry
@new:
    ldy b1NearCount
    cpy #TELEPORT_NEAR_MAX
    bcs @full
    inc b1NearCount
@entry:
    lda b1ConditionArgs + 2
    sta b1NearVar,y
    phy
    lda _teleportPending        ; set while parking: objects not placed yet
    bne @unknown
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportObjectAt
    bcc @store
@unknown:
    lda #$FF                    ; unknown or hidden
    tax
@store:
    ply
    sta b1NearX,y
    txa
    sta b1NearY,y
@other:
    clc
@done:
    rts
@full:
    jsr b1TriggerPartial
    clc
    rts

; Carry set if action A does something a box should guard: flags (set ..
; toggle.v), new.room, inventory (get, get.v, drop), sound, print and
; display, program.control.
b1TriggerEffect:
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
b1TriggerConfirm:
    ldx b1FrameDepth
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
b1TriggerCall:
    tay
    beq @done
    cmp TELEPORT_VAR + 0
    beq @done
    ldx b1CallCount
@seen:
    dex
    bmi @new
    cmp b1Calls,x
    bne @seen
    rts
@new:
    ldx b1CallCount
    cpx #TELEPORT_CALLS_MAX
    bcs b1TriggerPartial
    sta b1Calls,x
    inc b1CallCount
@done:
    rts

; Something was left unseen.
b1TriggerPartial:
    sec
    ror b1Partial
    rts

; A goto that ends an if block and jumps forward, but not past the end of
; the enclosing block, begins its else block. One that leaves the enclosing
; block is an early exit, not an else. In: A = offset high byte, b1GotoLow.
b1TriggerElse:
    cmp #$80
    bcs @done                   ; backward: a loop, not an else
    ldx b1FrameDepth
    beq @done
    pha
    jsr b1TriggerFrame          ; Y = top frame
    lda b1CodePosition
    cmp (ptr3),y
    bne @pull
    iny
    lda b1CodePosition + 1
    cmp (ptr3),y
    bne @pull
    lda b1CodePosition          ; else end = here + offset
    clc
    adc b1GotoLow
    sta b1PushEnd
    pla
    adc b1CodePosition + 1
    sta b1PushEnd + 1
    cpx #2
    bcc @else
    tya                         ; the enclosing block's end
    sec
    sbc #TELEPORT_FRAME_SIZE + 1
    tay
    lda (ptr3),y
    cmp b1PushEnd
    iny
    lda (ptr3),y
    sbc b1PushEnd + 1
    bcc @done                   ; it ends first: an early exit
@else:
    jsr b1TriggerFrame
    iny
    iny
    lda b1PushEnd
    sta (ptr3),y
    iny
    lda b1PushEnd + 1
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
b1TriggerFrame:
    lda b1FrameDepth
    dec a
    asl a
    sta b1FrameTemp
    asl a
    adc b1FrameTemp
    adc #TELEPORT_CODE_BUFFER
    tay
    rts

; A = FRAME_ACTIVE if code here can run (every enclosing block might).
b1TriggerActive:
    lda b1FrameDepth
    beq @top
    jsr b1TriggerFrame
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
b1TriggerUnwind:
    lda b1FrameDepth
    beq @done
    jsr b1TriggerFrame
    lda b1CodePosition
    cmp (ptr3),y
    iny
    lda b1CodePosition + 1
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
    bra b1TriggerUnwind
@pop:
    dec b1FrameDepth
    bra b1TriggerUnwind
@done:
    rts

; Pushes the if block whose condition is in b1Condition. Carry set if too deep.
b1TriggerPush:
    jsr b1TriggerByte           ; block size
    bcs @done
    sta b1GotoLow
    jsr b1TriggerByte
    bcs @done
    pha
    jsr b1TriggerActive
    asl a
    asl a
    asl a                       ; FRAME_ENCLOSING
    sta b1ConditionTemp
    lsr a
    lsr a
    lsr a
    and b1Condition             ; active: enclosing and might be true
    bne @active
    lda b1ZoneStart             ; its boxes cannot fire
    sta b1ZoneCount
    lda #TELEPORT_NO_BOX
    sta b1BoxIndex
    lda #0
@active:
    ora b1ConditionTemp
    sta b1ConditionTemp
    lda b1Condition
    and #CAN_FALSE
    ora b1ConditionTemp
    sta b1ConditionTemp
    lda b1FrameDepth
    cmp #TELEPORT_FRAMES
    bcs @deep
    inc b1FrameDepth
    jsr b1TriggerFrame
    lda b1CodePosition          ; end = here + size
    clc
    adc b1GotoLow
    sta (ptr3),y
    iny
    pla
    adc b1CodePosition + 1
    sta (ptr3),y
    iny
    iny
    iny
    lda b1ConditionTemp
    sta (ptr3),y
    iny
    lda b1BoxIndex
    sta (ptr3),y
    clc
@done:
    rts
@deep:
    pla
    sec
    rts

; Reads a condition up to its closing $FF into b1Condition (CAN_* bits),
; adding its boxes on ego. Carry set at the end of the code or on an
; unknown test.
b1TriggerCondition:
    lda b1ZoneCount
    sta b1ZoneStart
    lda #CAN_TRUE
    sta b1Condition
    stz b1ConditionInOr
    stz b1ConditionNot
    stz b1Box                   ; the whole screen, until narrowed
    stz b1Box + 1
    lda #TELEPORT_WIDTH - 1
    sta b1Box + 2
    lda #TELEPORT_HEIGHT - 1
    sta b1Box + 3
    stz b1BoxState
    bra @test
@closed:                        ; a narrowed position box is added, pending
    ldx #TELEPORT_NO_BOX
    lda b1BoxState
    cmp #1
    bne @store
    ldx b1ZoneCount
    phx
    ldx #b1Box - b1ConditionArgs
    jsr b1TriggerAddBox
    plx
    bcc @store
    ldx #TELEPORT_NO_BOX
@store:
    stx b1BoxIndex
    clc
    rts
@failed:
    sec
    rts
@test:
    jsr b1TriggerByte
    bcs @failed
    cmp #$FF
    beq @closed
    cmp #$FC                    ; or( ... )
    bne @notOr
    lda b1ConditionInOr
    eor #1
    sta b1ConditionInOr
    beq @closeOr
    lda #CAN_FALSE
    sta b1ConditionOr
    bra @test
@closeOr:
    lda b1ConditionOr
    jsr b1TriggerAnd
    bra @test
@notOr:
    cmp #$FD                    ; not
    bne @notNot
    sta b1ConditionNot
    bra @test
@notNot:
    cmp #$0E                    ; said(count, words...)
    bne @fixed
    jsr b1TriggerByte
    bcs @failed
    asl a
    jsr b1TriggerSkip
    bcs @failed
    lda #CAN_FALSE
    bra @value
@fixed:
    cmp #TELEPORT_TESTS
    bcs @failed
    sta b1ConditionTest
    jsr b1TestArgumentCount
    sta b1ArgumentCount
    beq @evaluate
    ldx #0
@argument:
    jsr b1TriggerByte           ; preserves X
    bcs @failed
    sta b1ConditionArgs,x
    inx
    cpx b1ArgumentCount
    bcc @argument
@evaluate:
    jsr b1TriggerEvaluate
@value:
    ldx b1ConditionNot
    beq @combine
    stz b1ConditionNot
    cmp #CAN_TRUE | CAN_FALSE
    beq @combine
    eor #CAN_TRUE | CAN_FALSE
@combine:
    ldx b1ConditionInOr
    bne @or
    jsr b1TriggerAnd
    bra @test
@or:
    tax                         ; or: might be true if either might,
    ora b1ConditionOr           ; false only if both might be
    and #CAN_TRUE
    sta b1ConditionTemp
    txa
    and b1ConditionOr
    and #CAN_FALSE
    ora b1ConditionTemp
    sta b1ConditionOr
    jmp @test

; b1Condition = b1Condition and A.
b1TriggerAnd:
    tax
    and b1Condition
    and #CAN_TRUE
    sta b1ConditionTemp
    txa
    ora b1Condition
    and #CAN_FALSE
    ora b1ConditionTemp
    sta b1Condition
    rts

; A = CAN_* outcomes of test b1ConditionTest with b1ConditionArgs.
b1TriggerEvaluate:
    lda b1ConditionTest
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
    lda b1ConditionArgs         ; on ego: a box to avoid. Its own test is
    bne @maybe                  ; taken as true; under not it is unknown.
    lda b1ConditionNot
    bne @maybe
    ldx #1
    jsr b1TriggerAddBox
@true:
    lda #CAN_TRUE
    rts
@isset:
    ldx b1ConditionArgs
    lda TELEPORT_FLAG,x
    bra @bool
@issetv:
    ldx b1ConditionArgs
    lda TELEPORT_VAR,x
    tax
    lda TELEPORT_FLAG,x
@bool:
    bne @true
@false:
    lda #CAN_FALSE
    rts
@compare:
    ldx b1EgoKnown              ; a variable holding ego's position?
    beq @near
    ldx b1ConditionArgs
    cpx b1EgoX
    beq @onX
    cpx b1EgoY
    bne @near
    ldx #1
    .byte $2C                   ; bit abs: skip the ldx
@onX:
    ldx #0
    jsr b1TriggerAxis
    bra @maybe
@near:
    ldx b1NearCount             ; ... or ego's distance to an object?
@nearest:
    dex
    bmi @exact
    lda b1NearVar,x
    cmp b1ConditionArgs
    bne @nearest
    jsr b1TriggerNear
    bra @maybe
@exact:
    lda b1ConditionTest
    lsr a                       ; odd: number form
    lda b1ConditionArgs + 1
    bcs @right
    tax
    lda TELEPORT_VAR,x
@right:
    sta b1ConditionTemp
    ldx b1ConditionArgs
    lda TELEPORT_VAR,x
    cmp b1ConditionTemp
    php
    lda b1ConditionTest
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

; Adds the box at b1ConditionArgs + X (x1 y1 x2 y2). Carry set if the list
; is full: then the walk is partial.
b1TriggerAddBox:
    lda b1ZoneCount
    cmp #TELEPORT_ZONES_MAX
    bcc @room
    jsr b1TriggerPartial
    sec
    rts
@room:
    asl a
    asl a
    tay
    iny                         ; zones + 1 + 4 * count
@corner:
    lda b1ConditionArgs,x
    sta (ptr1),y
    inx
    iny
    tya
    and #3
    cmp #1
    bne @corner                 ; four corners: up to the next 4k + 1
    inc b1ZoneCount
    clc
    rts

; Narrows the condition's ego-position box on axis X (0: X, 1: Y) by test
; b1ConditionTest against number b1ConditionArgs + 1. Inside or(), against a
; variable, or a negated equality, the box cannot express it.
b1TriggerAxis:
    jsr b1TriggerBound          ; Y = n, A = 0 equal, 1 less, 2 greater
    bcs b1TriggerUnusable
    cmp #1
    beq @less
    bcs @greater
    lda b1ConditionNot          ; equaln: low = high = n
    bne b1TriggerUnusable
    tya
    jsr b1TriggerRaise
    tya
    bra @lower
@less:
    lda b1ConditionNot          ; not lessn: x >= n
    beq @below
    tya
    bra @raise
@below:
    tya                         ; lessn: x <= n - 1
    beq b1TriggerUnusable
    dec a
    bra @lower
@greater:
    lda b1ConditionNot          ; not greatern: x <= n
    beq @above
    tya
    bra @lower
@above:
    tya                         ; greatern: x >= n + 1
    inc a
    beq b1TriggerUnusable
@raise:
    jsr b1TriggerRaise
    bra b1TriggerNarrowed
@lower:
    jsr b1TriggerLower
    ; Fall through.
b1TriggerNarrowed:
    lda #1
    .byte $2C                   ; bit abs: skip the lda
b1TriggerUnusable:
    lda #$80
    tsb b1BoxState
    rts

; Narrows the condition's box to near object entry X: ego within r of it,
; for lessn n (r = n - 1), equaln n and not greatern n (r = n). Being far
; from it, or not knowing where it is, cannot be a box.
b1TriggerNear:
    lda b1NearX,x
    cmp #$FF
    beq b1TriggerUnusable
    jsr b1TriggerBound
    bcs b1TriggerUnusable
    cmp #1
    beq @less
    bcs @greater
    lda b1ConditionNot          ; equaln: r = n
    bne b1TriggerUnusable
    bra @radius
@less:
    lda b1ConditionNot
    bne b1TriggerUnusable
    dey                         ; lessn: r = n - 1
    cpy #$FF
    beq b1TriggerUnusable
    bra @radius
@greater:
    lda b1ConditionNot          ; not greatern: r = n
    beq b1TriggerUnusable
@radius:
    sty b1NearRadius
    phx
    lda b1NearX,x
    ldx #0
    jsr @span
    plx
    lda b1NearY,x
    ldx #1
    jsr @span
    bra b1TriggerNarrowed
@span:                          ; axis X: centre A +- radius, on screen
    pha
    sec
    sbc b1NearRadius
    bcs @low
    lda #0
@low:
    jsr b1TriggerRaise
    pla
    clc
    adc b1NearRadius
    bcc b1TriggerLower
    lda #$FF
    ; Fall through.
; high = min(high, A) on axis X.
b1TriggerLower:
    cmp b1Box + 2,x
    bcs @done
    sta b1Box + 2,x
@done:
    rts

; low = max(low, A) on axis X.
b1TriggerRaise:
    cmp b1Box,x
    bcc @done
    sta b1Box,x
@done:
    rts

; A number test on a tracked variable: Y = n, A = 0 equaln, 1 lessn,
; 2 greatern. Carry set if the box cannot use it: inside or(), or the
; variable form.
b1TriggerBound:
    lda b1ConditionInOr
    cmp #1                      ; inside or(): carry set
    bcs @done
    ldy b1ConditionArgs + 1
    lda b1ConditionTest
    lsr a                       ; carry: number form
    bcc @variable
    clc
    rts
@variable:
    sec
@done:
    rts

; Argument bytes per command, two per byte (even command in the high nibble).
; Action commands 0-181 (AGI 2), from AGI Studio's command table.
b1ActionArguments:
    .byte $01, $12, $22, $22, $22, $22, $11, $11, $11, $11, $11, $11, $11, $01, $10, $11
    .byte $11, $01, $13, $33, $32, $22, $21, $12, $22, $22, $22, $22, $12, $11, $11, $11
    .byte $11, $11, $13, $11, $12, $12, $21, $12, $25, $53, $11, $22, $11, $40, $11, $12
    .byte $22, $12, $01, $13, $33, $00, $12, $13, $00, $25, $21, $20, $03, $77, $00, $00
    .byte $01, $30, $01, $10, $00, $00, $00, $11, $10, $03, $30, $34, $41, $52, $12, $01
    .byte $10, $10, $02, $22, $20, $10, $00, $11, $01, $04, $20
; Test commands 0-18; said (14) is variable and handled apart.
b1TestArguments:
    .byte $02, $22, $22, $21, $11, $25, $10, $02, $55, $50
.endif
