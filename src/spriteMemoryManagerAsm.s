.ifndef SPRITE_MEMORY_MANAGER_INC
SPRITE_MEMORY_MANAGER_INC = 1

.include "globalGraphics.s"
.include "globalViews.s"

.importzp sreg

.import popa

.segment "BANKRAM0E"
SPRITE_ALLOC_TABLE_SIZE = (SPRITE_END - SPRITE_START) / SEGMENT_SMALL
RESULT_SIZE = 20

_bESpriteAllocTable: .res SPRITE_ALLOC_TABLE_SIZE, $0

_bESpriteAddressTableMiddle: .res SPRITE_ALLOC_TABLE_SIZE, $0 ; Low will always be zero, hence no need for a table



; Macro: ALLOCATE_SPRITE_MEMORY_32
; Purpose: Allocate 32-byte segments for sprite memory in a CommanderX16 environment.
; This macro searches through a sprite allocation table to find an empty slot.
; Upon finding one, it sets up the sprite allocation and updates relevant pointers.
; Handles cases where the allocation slot is already filled and wraps around the allocation table if necessary.
; It returns with appropriate values to indicate successful or failed allocation.
.macro ALLOCATE_SPRITE_MEMORY_32
    .local @loop, @found, @prepareResult, @increaseWall, @greater, @lesser
    .local @nonEmpty, @return, @returnFail

    ; Initialize X and Y registers
    ldx #$0 ; X register: Indicates never reset to zero
    ldy ZP_PTR_SEG_32 ; Y register: Load segment pointer for 32-byte allocation
    lda ZP_PTR_WALL_32 ; A register: Load wall pointer for 32-byte allocation

    ; Initialize X and Y registers
    ldx #$0 ; X register: Indicates never reset to zero
    ldy ZP_PTR_SEG_32 ; Y register: Load segment pointer for 32-byte allocation
    lda ZP_PTR_WALL_32 ; A register: Load wall pointer for 32-byte allocation

    @loop:
        cpy ZP_PTR_WALL_64
        beq @resetAtZero
        lda _bESpriteAllocTable, y
        bne @nonEmpty

    @found:
        ; Allocation found, prepare for return
        lda ZP_PTR_WALL_32
        cmp ZP_PTR_SEG_32
        bne @prepareResult

    @increaseWall:
        ; Increase wall pointer if at end of allocation table
        inc ZP_PTR_WALL_32

    @prepareResult:
        ; Set allocation table entry to indicate allocated
        lda #$1
        sta _bESpriteAllocTable, y

        ; Load sprite address into X register
        ldx _bESpriteAddressTableMiddle, y

        ; Update segment pointer
        tya 
        iny 
        sty ZP_PTR_SEG_32

        ; Check if high byte start is reached
        cmp ZP_PTR_HIGH_BYTE_START
        bcs @greater

    @lesser:
        ; If not reached, set Y to 0 and return
        ldy #$0
        bra @return

    @greater:
        ; If reached, set Y to 1 and return
        ldy #$1
        bra @return

    @nonEmpty:
        ; If current slot is not empty, check next slot
        iny 
        cpy ZP_PTR_SEG_32
        beq @returnFail

        bra @loop

    @resetAtZero:
        ; Reset to start of allocation table if needed
        cpx #$0
        bne @returnFail
        ldx #$1

        ldy #$0
        bra @loop

    @returnFail:
        ; Return failure
        sty ZP_PTR_SEG_32
        ldy #0
        ldx #0

    @return:
        ; Return, low byte is always zero
        lda #$0
.endmacro


;low a middle x high y

ZP_PTR_WALL_32_PLUS_3 = ZP_TMP_2
; Macro: ALLOCATE_SPRITE_MEMORY_64
; Purpose: Allocate 64-byte segments for sprite memory in a CommanderX16 environment.
; Similar to the 32-byte allocator but handles 64-byte segments.
; Searches the sprite allocation table for an empty slot and prepares it for sprite allocation.
; Adjusts the wall pointer as necessary and manages wrapping around the table.
; Returns with values indicating the outcome of the allocation attempt (success or failure).
.macro ALLOCATE_SPRITE_MEMORY_64
    .local @loop, @goBack, @checkResultTable, @found, @prepareResult, @storeSegmentPointer, @increaseWall, @zeroWall
    .local @greater, @lesser, @nonEmpty, @return, @returnFail, @resetToEnd

    lda ZP_PTR_WALL_32
    inc
    inc
    inc 
    sta ZP_PTR_WALL_32_PLUS_3

    ; Initialize X and Y registers
    ldx #$0 ; X register: Indicates never reset to zero
    ldy ZP_PTR_SEG_64 ; Y register: Load segment pointer for 64-byte allocation

    @loop:
        cpy ZP_PTR_WALL_32 ; Check if wall pointer is at end of allocation table
        beq @resetToEnd
        
        cpy ZP_PTR_WALL_32_PLUS_3 ; As we are four in size non of our 4 segments can be less than 32
        bcc @checkResultTable ; If we have jumped over the 32 wall we need to go back two. This is a unique problem for 64 bit alloc, as we jump in 2s
        
        lda ZP_PTR_WALL_32 ;Special case if ZP_PTR_WALL_32 is zero then we are perfectly entitled to allocated ourselves and 1 and 2, because it means 32 hasn't allocated anything yet and we won't overwrite anything
        beq @checkResultTable

        @goBack:
        iny 
        iny
        iny
        iny
        bra @resetToEnd

    @checkResultTable:
        ; Check if current slot is empty
        lda _bESpriteAllocTable, y
        bne @nonEmpty

    @found:
        ; Allocation found, prepare for return
        lda ZP_PTR_WALL_64
        cmp ZP_PTR_SEG_64
        bne @prepareResult

    @increaseWall:
        ; Decrease wall pointer if at end of allocation table
        dec ;Note a already holds ZP_PTR_WALL_64
        dec
        dec
        dec
        sta ZP_PTR_WALL_64
        cmp #$FF ;If the wall has gone below 0 then the wall should be set to zero
        bne @prepareResult

    @zeroWall:
        lda #$1 ;We cannot use zero, as there are an odd number number of allocation bytes. We use 134 and 133 ... 2 and 1
        sta ZP_PTR_WALL_64
    @prepareResult:
        ; Set allocation table entry to indicate allocated
        lda #$1
        sta _bESpriteAllocTable, y

        ; Load sprite address into X register
        ldx _bESpriteAddressTableMiddle, y

        ; Update segment pointer
        tya 
        dey
        dey
        dey
        dey

        cpy #$FF ; If we have gone below 0 then we need to wrap around
        bne @storeSegmentPointer
        ldy #SPRITE_ALLOC_TABLE_SIZE - 2

    @storeSegmentPointer:
        sty ZP_PTR_SEG_64

        ; Check if high byte start is reached
        cmp ZP_PTR_HIGH_BYTE_START
        bcs @greater

    @lesser:
        ; If not reached, set Y to 0 and return
        ldy #$0
        bra @return

    @greater:
        ; If reached, set Y to 1 and return
        ldy #$1
        bra @return

    @nonEmpty:
        ; If current slot is not empty, check next slot
        dey
        dey
        cpy ZP_PTR_SEG_64
        beq @returnFail

        bra @loop

    @resetToEnd:
        ; Reset to end of allocation table if needed
        cpx #$0
        bne @returnFail
        ldx #$1

        ldy #SPRITE_ALLOC_TABLE_SIZE - 4
        bra @loop

    @returnFail:
        ; Return failure
        ldy #SPRITE_ALLOC_TABLE_SIZE - 4
        sty ZP_PTR_SEG_64
        ldy #0
        ldx #0

    @return:
        ; Return, low byte is always zero
        lda #$0
.endmacro





_bEAllocateSpriteMemory32:
ALLOCATE_SPRITE_MEMORY_32
sty sreg
stz sreg + 1
rts

_bEAllocateSpriteMemory64:
ALLOCATE_SPRITE_MEMORY_64
sty sreg
stz sreg + 1
rts

ZP_SIZE = ZP_TMP_3 ;Starting from 3 as allocate functions use one tmp ZP
ZP_NUMBER_TO_ALLOCATE = ZP_TMP_4
ZP_ARRAY_COUNTER = ZP_TMP_5
;Puts the returned memory addresses on the system stack. Note as the lower byte is always zero only the high and middle bytes (in that order) are pushed onto the stack
; void bEAllocateSpriteMemoryBulk(AllocationSize size, byte number) AllocationSize is 0 for 32 and 1 for 64
_bEBulkAllocatedAddresses: .res VIEW_TABLE_SIZE * VERA_ADDRESS_SIZE + 1 * ALLOCATOR_BLOCK_SIZE_64, $0 ;64 is the biggest size, so if we have enough room for that the others will fit anyway. Add 1 as C will read these as longs

_bEAllocateSpriteMemoryBulk:
sta ZP_NUMBER_TO_ALLOCATE ;Save the number of sprites to allocate
jsr popa
sta ZP_SIZE ;Save the size of the allocation

stz ZP_ARRAY_COUNTER

@loop:
lda ZP_SIZE
bne @64Alloc
@32Alloc:
ALLOCATE_SPRITE_MEMORY_32
;To Do if we cannot find a 32 slot we should ask for a 64 slot. But to test that we really need delete first, otherwise there will be guaranteed to be no 64 slots anyway
bra @storeAndDecrementCounter
@64Alloc:
ALLOCATE_SPRITE_MEMORY_64

@storeAndDecrementCounter:
tya
ldy ZP_ARRAY_COUNTER

sta _bEBulkAllocatedAddresses + 2, y ;Store the high byte
txa
sta _bEBulkAllocatedAddresses + 1, y ;Store the middle byte

ora _bEBulkAllocatedAddresses + 2 ;Oring middle and high byte together
beq @returnFail ;If the high byte is zero and the middle byte is also zero then we have failed to allocate, due lack of memory

lda #$0
sta _bEBulkAllocatedAddresses, y ;Store the low byte
sta _bEBulkAllocatedAddresses + 3, y ;Store high byte of the C long. This is always zero

iny
iny
iny
iny

sty ZP_ARRAY_COUNTER

dec ZP_NUMBER_TO_ALLOCATE
beq @return
jmp @loop


@return:
lda #$1
rts

@returnFail:
lda #$0
rts
.endif