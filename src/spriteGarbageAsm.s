.ifndef GARBAGE_INC
GARBAGE_INC = 1

.include "helpersAsm.s"

;------------------------------------------------------------------------------
; External Imports (Offsets, Routines, and Definitions)
;------------------------------------------------------------------------------
.import _offsetOfloopsVeraAddressesPointers
.import _offsetOfViewMetadataBank
.import _offsetOfNumberOfLoops
.import _offsetOfMaxCels
.import _offsetOfMaxVeraSlots
.import _offsetOfCurrentView
.import _offsetOfCurrentLoop
.import _offsetOfFlags
.import _offsetOfBackBuffers
.import _offsetOfDirection
.import _offsetOfIsOnBackBuffer
.import _offsetOfDirection

.import _b5Multiply

.import _sizeOfViewTab
.import _sizeOfViewTableMetadata
.import _sizeOfView
.import _viewtab
.import _viewTableMetadata
.import _loadedViews

.import _bAViewTableQuickLookupLow
.import _bAViewTableQuickLookupHigh
.import _bAViewTableMetdataQuickLookupLow
.import _bAViewTableMetdataQuickLookupHigh
.import _bAViewQuickLookupLow
.import _bAViewQuickLookupHigh

.import _viewTabNoToMetaData

;------------------------------------------------------------------------------
; Zero-Page Variables (Used by the Sprite Garbage Collector)
;------------------------------------------------------------------------------
.segment "ZEROPAGE"
SGC_LOOP_VERA_ADDR:       .res 2  ; Pointer to array of VERA addresses for loops
SGC_CEL_VERA_ADDR:        .res 2  ; Pointer to current cel’s VERA address

VIEW_TAB_COUNTER:         .res 1  ; Index/counter for the view table
LOOPS_COUNTER_ADDRESS:    .res 1  ; Counter for loop iteration
CEL_COUNTER_ADDRESS:      .res 1  ; Counter for cel iteration

; These are also used in "CelToVeraBulk" due to limited ZP space
SGC_VIEW_METADATA:        .res 2
SGC_LOOP_VERA_ADDR_BANK:  .res 1
SGC_CURRENT_LOOP:         .res 1
SGC_CLEAR_ACTIVE_LOOP:    .res 1
SGC_LOCAL_VIEW:           .res 2
SGC_NO_LOOPS:             .res 1
SGC_MAX_CELS:             .res 1
SGC_VIEW_TAB:             .res 2
SGC_FLAGS:                .res 2
SGC_BACKBUFFERS:          .res 2  ; Pointers to back buffers (reused space)
SGC_BACKBUFFER_VERA_ADDRESS: .res 2
SGC_MAX_VERA_SLOTS:       .res 1
SGC_BACK_BUFFER_COUNTER:  .res 1
SGC_MEMORY_CLEARED:       .res 1
_SGC_LAST_LOCATION_GC_CHECKED: .res 1
INCREMENTAL_GARBAGE_COLLECTOR_COUNTER: .res 1

.segment "CODE"

;------------------------------------------------------------------------------
; Flags (bitmasks) for checking if a view is animated and drawn
;------------------------------------------------------------------------------
ANIMATED_AND_DRAWN = ANIMATED | DRAWN

;------------------------------------------------------------------------------
; deleteSpriteMemoryForViewTab
; 
; Core routine that deletes sprite memory for a single ViewTab entry:
;   - Reads data from RAM bank
;   - Checks if the current loop should be removed or retained
;   - Iterates through loops/cels, clearing them if needed
;   - Optionally processes back-buffer memory
;------------------------------------------------------------------------------
deleteSpriteMemoryForViewTab:
    ; Save current RAM bank to restore later
    lda RAM_BANK
    sta @previousRamBank

    ; Switch to VIEW_TAB_BANK to read from the view table
    lda #VIEW_TAB_BANK
    sta RAM_BANK

    ; Load flags from the view table
    GET_STRUCT_8_STORED_OFFSET _offsetOfFlags, SGC_VIEW_TAB
    and #ANIMATED_AND_DRAWN
    sec
    sbc #ANIMATED_AND_DRAWN
    sta SGC_CLEAR_ACTIVE_LOOP
    bne @initLoopsLoop

    ; If SGC_CLEAR_ACTIVE_LOOP == 0, we increment SGC_CURRENT_LOOP.
    ; Since each loop address is two bytes, we ASL (multiply by 2) before INC.
    lda SGC_CURRENT_LOOP
    asl          ; multiply the loop index by 2 (each loop is two bytes)
    inc          ; move to the next 2-byte entry
    sta SGC_CURRENT_LOOP
@initLoopsLoop:
    lda SGC_NO_LOOPS ;Load the loop count to prepare for backwards iteration
    asl ;Double it as there are two bytes per loop address         
    dec ;Zero indexed -1
@loopsLoop:
    tay ;Will be used for index register
    tax ;Used down below to compare to the current loop

    ; Switch to the bank that contains the loop addresses
    lda SGC_LOOP_VERA_ADDR_BANK
    sta RAM_BANK

    ; Load high byte
    lda (SGC_LOOP_VERA_ADDR),y
    sta SGC_CEL_VERA_ADDR + 1
    dey
    ; Load low byte
    lda (SGC_LOOP_VERA_ADDR),y
    sta SGC_CEL_VERA_ADDR
    dey
    ; Save loop counter
    sty LOOPS_COUNTER_ADDRESS

    ; If forced to clear all loops, skip the check
    lda SGC_CLEAR_ACTIVE_LOOP
    bne @initCelsLoop
    ; Otherwise, only clear if not the current loop
    cpx SGC_CURRENT_LOOP
    beq @checkLoopsLoop

@initCelsLoop:
    lda SGC_MAX_CELS        ; Load the loop count to prepare for backwards iteration 
    asl                     ; Double it as there are two bytes per  address  
    dec                     ; Zero indexed -1
@celsLoop:
    ; Use Y as the offset to the current cel pointer
    tay                             

    ;-----------------------------------------
    ; HIGH BYTE OF CURRENT CEL POINTER
    ;-----------------------------------------
    lda (SGC_CEL_VERA_ADDR),y        ; Read the high byte of the cel pointer
    tax                              ; Store it in X for check if cel pointer was zero check AND for deallocation of sprite memory
    lda #$0                          ; Prepare to zero it out
    sta (SGC_CEL_VERA_ADDR),y        ; Clear the high byte in memory
    dey                              ; Move Y back one step for the low byte

    ;-----------------------------------------
    ; LOW BYTE OF CURRENT CEL POINTER
    ;-----------------------------------------
    lda (SGC_CEL_VERA_ADDR),y        ; Read the low byte of the cel pointer
    sta sreg                         ; Put it into 'sreg' (scratch)
    lda #$0                          
    sta (SGC_CEL_VERA_ADDR),y        ; Clear the low byte in memory

    dey                              ; Decrement Y again
    sty CEL_COUNTER_ADDRESS          ; Track how many cels processed so far

    ;-----------------------------------------
    ; CHECK IF CEL POINTER WAS ZERO
    ;-----------------------------------------
    txa                              ; Combine the high byte (X) ...
    ora sreg                         ; ... with the low byte (sreg)
    beq @checkCelsLoop               ; If combined == 0, no memory to free => skip

    ;-----------------------------------------
    ; DEALLOCATE SPRITE MEMORY FOR THIS CEL
    ;-----------------------------------------
    lda sreg                         ; Load the low byte again. The high byte is already in x
    ldy #GARBAGE_BANK                ; Switch to the 'garbage' bank
    sty RAM_BANK
    jsr bADeallocSpriteMemory        ; Free the sprite memory

    lda #$1
    sta SGC_MEMORY_CLEARED           ; Indicate that some memory was cleared

@checkCelsLoop:
    ; Restore the loop address bank
    lda SGC_LOOP_VERA_ADDR_BANK
    sta RAM_BANK

    ; Check if we still have cels left to process
    lda CEL_COUNTER_ADDRESS
    bpl @celsLoop
    
@checkForBackBuffer: ;Remove backbuffer for cels
    ; Switch to SPRITE_METADATA_BANK for reading back-buffers
    lda #SPRITE_METADATA_BANK
    sta RAM_BANK

    ; Load the back-buffer pointer from the view metadata
    GET_STRUCT_16_STORED_OFFSET _offsetOfBackBuffers, SGC_VIEW_METADATA, SGC_BACKBUFFERS
    ora SGC_BACKBUFFERS
    beq @checkLoopsLoop

    ; Switch back to the loop bank
    lda SGC_LOOP_VERA_ADDR_BANK
    sta RAM_BANK

    lda (SGC_BACKBUFFERS)
    ldy #$1
    ora (SGC_BACKBUFFERS),y
    beq @checkLoopsLoop ;If we don't have a backbuffer we have no need to bother deleting it, goto next loop

    jmp @checkIfBackBufferCanBeDeleted ;Go to backbuffer delete

@checkLoopsLoop:
    ; We are done with this loop
    lda LOOPS_COUNTER_ADDRESS
    bpl @loopsLoop

    ; Mark the view tab as free in _viewsWithSpriteMem
    ldx VIEW_TAB_COUNTER
    stz _viewsWithSpriteMem,x

    ; Restore original RAM bank and exit
    lda @previousRamBank
    sta RAM_BANK
    rts

@previousRamBank: .byte $0

;------------------------------------------------------------------------------
; @checkIfBackBufferCanBeDeleted
;
; PURPOSE:
;   This routine verifies if we can safely delete the back-buffer memory
;   allocated for the current view. In some cases, we may only remove
;   the back-buffer if certain conditions (listed below) are satisfied.
;
; CONDITIONS TO FREE BACK-BUFFER:
;   1) If SGC_CLEAR_ACTIVE_LOOP is non-zero, we do not need any further checks;
;      we skip directly to freeing everything.
;   2) We must not be operating on the "current loop" in use (SGC_CURRENT_LOOP);
;      if we are, we leave the back-buffer alone for safety.
;   3) The view must not be in motion (checked via the MOTION bit in SGC_FLAGS).
;   4) The view must not be flagged as "isOnBackBuffer" in its metadata.
;   5) The direction in the view tab must be zero (no movement).
;
; If all the above checks are passed (or SGC_CLEAR_ACTIVE_LOOP != 0),
; then we remove the back-buffer by iterating over each allocated slot
; and calling the deallocation routine. Once freed, we clear the pointers
; in the SGC_BACKBUFFERS array.
;
; FLOW:
;   - If forced clearing is signaled, jump straight to @initBackBufferLoop.
;   - Otherwise, check each condition in turn and jump away (to @checkLoopsLoop)
;     if any condition says "do not remove back-buffer".
;   - If we pass all checks, drop into @initBackBufferLoop to perform deletion.
;------------------------------------------------------------------------------
@checkIfBackBufferCanBeDeleted:
    ;------------------------------------------------------------------
    ; 1) CHECK IF WE ARE FORCED TO CLEAR ALL LOOPS
    ;------------------------------------------------------------------
    lda SGC_CLEAR_ACTIVE_LOOP
    bne @initBackBufferLoop    ; If non-zero => forcibly free; skip checks

    ;------------------------------------------------------------------
    ; 2) CHECK IF WE ARE ON THE SAME LOOP AS THE CURRENT LOOP
    ;    If the "active" loop matches the loop counter, we must NOT
    ;    remove the back-buffer for safety.
    ;------------------------------------------------------------------
    lda SGC_CURRENT_LOOP
    cmp LOOPS_COUNTER_ADDRESS
    beq @checkLoopsLoop        ; Same loop => skip removal

    ;------------------------------------------------------------------
    ; 3) CHECK THE MOTION BIT
    ;    If the view is in motion (SGC_FLAGS & MOTION != 0), skip removing.
    ;------------------------------------------------------------------
    lda SGC_FLAGS
    and #MOTION
    bne @checkLoopsLoop        ; In motion => skip

    ;------------------------------------------------------------------
    ; 4) CHECK IF THE VIEW IS MARKED AS "isOnBackBuffer"
    ;    If true, then skip removal. This is stored in the metadata.
    ;------------------------------------------------------------------
    GET_STRUCT_8_STORED_OFFSET _offsetOfIsOnBackBuffer, SGC_VIEW_METADATA
    bne @checkLoopsLoop        ; If set => skip

    ;------------------------------------------------------------------
    ; 5) CHECK THE VIEW'S DIRECTION
    ;    Must be zero to indicate no active movement. If direction != 0,
    ;    we skip removing back-buffer.
    ;------------------------------------------------------------------
    lda #VIEW_TAB_BANK
    sta RAM_BANK
    GET_STRUCT_8_STORED_OFFSET _offsetOfDirection, SGC_VIEW_TAB
    bne @checkLoopsLoop        ; direction != 0 => skip

;------------------------------------------------------------------------------
; If all checks above are passed or forced-clear is set,
; initialize the loop to free each allocated VERA slot in back-buffer.
;------------------------------------------------------------------------------
@initBackBufferLoop:
    ;------------------------------------------------------------------
    ; LOAD THE SGC_MAX_VERA_SLOTS
    ; This tells us how many VERA slots are allocated for back-buffer usage.
    ; We then shift left (ASL) and DEC to get our indexing range set up.
    ;------------------------------------------------------------------
    lda SGC_MAX_VERA_SLOTS
    asl ;Double it as there are two bytes per loop address   
    dec ;Zero indexed -1
    tay ;Use this to index backbuffer address 
    sta SGC_BACK_BUFFER_COUNTER

;------------------------------------------------------------------------------
; Loop that processes each VERA slot pointer in SGC_BACKBUFFERS.
;------------------------------------------------------------------------------
@backBufferLoop:
    ;------------------------------------------------------------------
    ; READ A TWO-BYTE POINTER FROM THE BACK-BUFFERS ARRAY
    ;   - LDA (SGC_BACKBUFFERS),y => Low byte of pointer
    ;   - store in sreg
    ;   - DEY => move to next byte
    ;   - LDA (SGC_BACKBUFFERS),y => High byte of pointer
    ;   - move it into X for bADeallocSpriteMemory usage.
    ;------------------------------------------------------------------
    lda (SGC_BACKBUFFERS),y
    tax             ;Store the high byte of the backbuffer for bADeallocSpriteMemory
    dey
    lda (SGC_BACKBUFFERS),y ;Store the low byte

    ;------------------------------------------------------------------
    ; SWITCH TO THE GARBAGE BANK & DEALLOCATE THIS SPRITE MEMORY
    ;------------------------------------------------------------------
    ldy #GARBAGE_BANK
    sty RAM_BANK
    jsr bADeallocSpriteMemory

    ; Set a flag indicating we cleared memory from at least one slot.
    lda #$1
    sta SGC_MEMORY_CLEARED

    ;------------------------------------------------------------------
    ; RESTORE THE PREVIOUS BANK (WHICH HOLDS LOOP INFO)
    ;------------------------------------------------------------------
    lda SGC_LOOP_VERA_ADDR_BANK
    sta RAM_BANK

    ;------------------------------------------------------------------
    ; ZERO OUT THIS SGC_BACKBUFFERS SLOT
    ; We overwrite both bytes with zero to indicate it's freed.
    ;------------------------------------------------------------------
    ldy SGC_BACK_BUFFER_COUNTER
    lda #$0
    sta (SGC_BACKBUFFERS),y
    dey
    sta (SGC_BACKBUFFERS),y
    dey

    ;------------------------------------------------------------------
    ; DECREMENT AND STORE THE UPDATED COUNTER
    ;------------------------------------------------------------------
    sty SGC_BACK_BUFFER_COUNTER

@checkBackbufferLoop:
    ;------------------------------------------------------------------
    ; IF COUNTER >= 0, WE STILL HAVE SLOTS TO PROCESS
    ;------------------------------------------------------------------
    bpl @backBufferLoop

    ;------------------------------------------------------------------
    ; ONCE DONE, RETURN TO THE LOOP-LEVEL LOGIC
    ;------------------------------------------------------------------
    jmp @checkLoopsLoop


;------------------------------------------------------------------------------
; _runIncrementalGarbageCollector
;
; PURPOSE:
;   This routine checks how much free sprite memory remains (by comparing
;   ZP_PTR_WALL_64 and ZP_PTR_WALL_32), and if the gap is too small (less
;   than or equal to MINIMUM_GAP), it triggers incremental garbage collection.
; 
; LOGIC FLOW:
;   1. Compare the difference (ZP_PTR_WALL_64 - ZP_PTR_WALL_32) to MINIMUM_GAP + 1.
;      - If we still have enough space (bcs => "branch if carry set"), skip GC entirely.
;   2. If not enough space, iterate through the _viewsWithSpriteMem array starting
;      from the position stored in _SGC_LAST_LOCATION_GC_CHECKED. This helps
;      spread out garbage collection across multiple calls.
;   3. For each view that has allocated sprite memory, call the core GC routine
;      (runSpriteGarbageCollectorAsmStart) once. If GC freed something (carry set),
;      exit early or proceed to the next.
;   4. Loop until:
;      - We have iterated through all views, or
;      - We’ve freed something in this pass, or
;      - We run out of iteration steps.
;   5. Store the updated index in _SGC_LAST_LOCATION_GC_CHECKED for next time,
;      and return.
;------------------------------------------------------------------------------
START = 1
INCREMENT = 5
MINIMUM_GAP = 5 * SEGMENT_LARGE_SPACES
.import _viewsWithSpriteMem

_runIncrementalGarbageCollector:
    sec                           ; Prepare for subtraction (SBC). 
                                  ; Set carry for "ZP_PTR_WALL_64 - ZP_PTR_WALL_32".

    lda ZP_PTR_WALL_64            ; Load the "64K pointer wall" (high memory boundary).
    sbc ZP_PTR_WALL_32            ; Subtract the "32K pointer wall" (low memory boundary).
    cmp #MINIMUM_GAP + 1          ; Compare difference to (MINIMUM_GAP + 1).
    bcs @return                   ; If carry set => difference >= (MINIMUM_GAP + 1),
                                  ;                meaning we have enough memory => skip GC.

@initRunCollectorLoop:
    ;--------------------------------------------------------------------
    ; We need to iterate over the entire "view table" (VIEW_TABLE_SIZE).
    ; We use Y as a countdown, and X as the current index into 
    ; _viewsWithSpriteMem.
    ;--------------------------------------------------------------------
    ldy #VIEW_TABLE_SIZE          ; Y = total number of possible views
    ldx _SGC_LAST_LOCATION_GC_CHECKED
                                  ; X = where we last left off in the table

@checkRunCollectorLoop:
    dey                           ; Decrement Y => we are scanning from the last point
    beq @end                      ; If Y == 0 => we've looped enough => end

@runCollectorLoop:
    inx                           ; Move to the next view index
    cpx #VIEW_TABLE_SIZE          ; If we've gone beyond the number of views...
    bcs @resetLastLocationGCChecked

@checkViewsWithSpriteMem:
    lda _viewsWithSpriteMem,x     ; Check if this view has allocated sprite memory
    beq @checkRunCollectorLoop    ; If zero => no memory allocated => skip

    ;--------------------------------------------------------------------
    ; If _viewsWithSpriteMem[x] != 0, we may have sprite memory to free.
    ; We'll attempt a single garbage collection pass for this view.
    ;--------------------------------------------------------------------
    stx _SGC_LAST_LOCATION_GC_CHECKED
                                  ; Record that we last checked this index
    sty INCREMENTAL_GARBAGE_COLLECTOR_COUNTER
                                  ; Store the 'remaining views' count in a ZP variable
    stx sgc_startNo
    stx sgc_endNo                 ; For this codebase, startNo == endNo => single view
    jsr runSpriteGarbageCollectorAsmStart                               ; Actually run the GC for this view
    bne @end                      ; If zero flag is set not set return, this means that something was cleared and we stop the collection, otherwise we move on to the next potential view tab

    ;--------------------------------------------------------------------
    ; If nothing was cleared:
    ; reload X and Y from memory
    ;--------------------------------------------------------------------
    ldx _SGC_LAST_LOCATION_GC_CHECKED
    ldy INCREMENTAL_GARBAGE_COLLECTOR_COUNTER

;Try the next view tab
    bra @checkRunCollectorLoop

@end:
    ;--------------------------------------------------------------------
    ; Store the final position we checked so next time we can pick up from here.
    ;--------------------------------------------------------------------
    stx _SGC_LAST_LOCATION_GC_CHECKED

@return:
    rts                           ; Return from the incremental GC

@resetLastLocationGCChecked:
    ;--------------------------------------------------------------------
    ; If we've gone beyond the last view (X >= VIEW_TABLE_SIZE),
    ; reset X to 1 so we continue from near the start next time.
    ;--------------------------------------------------------------------
    ldx #$1
    bra @checkViewsWithSpriteMem

;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
; _runSpriteGarbageCollector (ENTRY POINT #1 - Called from C)
;
; PURPOSE:
;   - Receives two parameters from C code:
;       1) 'end' in the Accumulator (A).
;       2) 'start' on the stack (popped via popa).
;   - Stores these parameters into the zero-page variables sgc_endNo and sgc_startNo.
;   - Immediately falls through to runSpriteGarbageCollectorAsmStart, which
;     runs the main garbage-collection logic.
;
; PARAMETERS:
;   A = end         (Upper bound for the view-tab index we want to GC)
;   [stack] = start (Lower bound for the GC range, popped into A by popa)
;
; FLOW:
;   1) 'end' is saved to sgc_endNo.
;   2) 'start' is popped from the stack, then stored to sgc_startNo.
;   3) Execution continues (no RTS here) directly into runSpriteGarbageCollectorAsmStart.
;------------------------------------------------------------------------------
_runSpriteGarbageCollector:
    sta sgc_endNo         ; Step 1: Store 'end' (in A) into sgc_endNo
    jsr popa              ; Step 2: Pop 'start' from the stack into A
    sta sgc_startNo       ; Store the popped 'start' value
    ; Fall through => no RTS => we jump directly into the next label:


.macro CALCULATE_MAX_CELS VIEW
    GET_STRUCT_8_STORED_OFFSET  _offsetOfMaxCels, VIEW
    tax
    GET_STRUCT_8_STORED_OFFSET  _offsetOfMaxVeraSlots, VIEW
    sta  SGC_MAX_VERA_SLOTS
    tay
    txa
    jsr  mul8x8to8               ; Multiply (maxCels * maxVeraSlots) => A
    sta  SGC_MAX_CELS
.endmacro

;------------------------------------------------------------------------------
; runSpriteGarbageCollectorAsmStart (ENTRY POINT #2 - The actual GC logic)
;
; PURPOSE:
;   - Iterates from sgc_endNo down to sgc_startNo (inclusive),
;     checking each view table entry to see if any sprite memory
;     (loops/cels) is allocated.
;   - If a view has allocated loops, we switch to the correct banks
;     and call deleteSpriteMemoryForViewTab to free them.
;   - Ends with A = SGC_MEMORY_CLEARED (0 if no memory freed, non-zero if memory was freed).
;
; FLOW:
;   1) Clear the "memory cleared" flag (SGC_MEMORY_CLEARED).
;   2) Save the current RAM bank, then switch to a "helpers" bank
;      so we can safely run cross-bank logic (lookups, multiplications, etc.).
;   3) Start at sgc_endNo (highest index) and iterate downward to sgc_startNo:
;       a) Switch to VIEW_TAB_BANK to find the ViewTab pointer,
;          read the current loop index.
;       b) Switch to SPRITE_METADATA_BANK to get the metadata pointer,
;          then read the loops pointer. If zero, skip.
;       c) Otherwise, load the loop bank, fetch the actual View index,
;          load the View struct from LOADED_VIEW_BANK, gather number of loops,
;          compute SGC_MAX_CELS, and then call deleteSpriteMemoryForViewTab.
;   4) Once we've reached sgc_startNo or processed all entries, restore the old bank.
;   5) Return with A = SGC_MEMORY_CLEARED (indicating whether memory was freed).
;------------------------------------------------------------------------------
runSpriteGarbageCollectorAsmStart:
    stz  SGC_MEMORY_CLEARED   ; 1) Reset "did we free anything?" flag to 0

    lda  RAM_BANK             ; 2) Save the current RAM bank...
    pha
    lda  #HELPERS_BANK
    sta  RAM_BANK             ; ...and switch to a helpers bank for advanced logic

    ldx  sgc_endNo            ; 3) We'll count down from sgc_endNo...
    stx  VIEW_TAB_COUNTER

@viewTabLoop:
    ldx  VIEW_TAB_COUNTER     ; Current index => X

    ;----------------------------------------------------------------------
    ;  a) Switch to the VIEW_TAB_BANK: we read pointers from an array
    ;     (_bAViewTableQuickLookupLow/High) that points to each ViewTab.
    ;----------------------------------------------------------------------
    lda  #VIEW_TAB_BANK
    sta  RAM_BANK
    lda  _bAViewTableQuickLookupLow,x
    sta  SGC_VIEW_TAB
    lda  _bAViewTableQuickLookupHigh,x
    sta  SGC_VIEW_TAB + 1

    ; Also fetch the "current loop" from this ViewTab
    GET_STRUCT_8_STORED_OFFSET _offsetOfCurrentLoop, SGC_VIEW_TAB, SGC_CURRENT_LOOP

    ;----------------------------------------------------------------------
    ;  b) Switch to SPRITE_METADATA_BANK: 
    ;     Load the metadata pointer for this view tab, then read the loop pointer.
    ;----------------------------------------------------------------------
    lda  #SPRITE_METADATA_BANK
    sta  RAM_BANK

    lda  _bAViewTableMetdataQuickLookupLow,x
    sta  SGC_VIEW_METADATA
    lda  _bAViewTableMetdataQuickLookupHigh,x
    sta  SGC_VIEW_METADATA + 1

    GET_STRUCT_16_STORED_OFFSET _offsetOfloopsVeraAddressesPointers, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR

    lda  SGC_LOOP_VERA_ADDR 
    ora  SGC_LOOP_VERA_ADDR + 1
    beq  @incrementCounter   ; If pointer == 0 => no loops => skip further checks

    ; Load the bank used for loops
    GET_STRUCT_8_STORED_OFFSET _offsetOfViewMetadataBank, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR_BANK

    ;----------------------------------------------------------------------
    ;  c) Switch back to VIEW_TAB_BANK to fetch the actual "view" index,
    ;     then jump to LOADED_VIEW_BANK to load the full View struct.
    ;----------------------------------------------------------------------
    lda  #VIEW_TAB_BANK
    sta  RAM_BANK
    GET_STRUCT_8_STORED_OFFSET _offsetOfCurrentView, SGC_VIEW_TAB
    tay                        ; Y = index into the array of loaded views

    lda  #LOADED_VIEW_BANK
    sta  RAM_BANK
    lda  _bAViewQuickLookupLow,y
    sta  SGC_LOCAL_VIEW
    lda  _bAViewQuickLookupHigh,y
    sta  SGC_LOCAL_VIEW + 1

    ; Now get the number of loops and compute how many cels in total we can have
    GET_STRUCT_16_STORED_OFFSET _offsetOfNumberOfLoops, SGC_LOCAL_VIEW, SGC_NO_LOOPS

    GET_STRUCT_8_STORED_OFFSET  _offsetOfMaxCels, SGC_LOCAL_VIEW
    tax
    GET_STRUCT_8_STORED_OFFSET  _offsetOfMaxVeraSlots, SGC_LOCAL_VIEW
    sta  SGC_MAX_VERA_SLOTS
    tay
    txa
    jsr  mul8x8to8               ; Multiply (maxCels * maxVeraSlots) => A
    sta  SGC_MAX_CELS

    ;----------------------------------------------------------------------
    ; Actually free the loop/cel memory by calling our core deallocation routine.
    ;----------------------------------------------------------------------
    jsr  deleteSpriteMemoryForViewTab

@incrementCounter:
    ;----------------------------------------------------------------------
    ; Decrement the view tab counter and compare against sgc_startNo:
    ;   If we've gone below sgc_startNo, we're done => exit.
    ; Otherwise, we keep iterating from high index to low index.
    ;----------------------------------------------------------------------
    lda  VIEW_TAB_COUNTER
    dec
    cmp  sgc_startNo
    sta  VIEW_TAB_COUNTER
    bmi  @endViewTabLoop
    jmp  @viewTabLoop

@endViewTabLoop:
    ;----------------------------------------------------------------------
    ; 4) Restore the original RAM bank that we saved earlier (PHA above).
    ;----------------------------------------------------------------------
    pla
    sta  RAM_BANK

    ;----------------------------------------------------------------------
    ; 5) Return with A = SGC_MEMORY_CLEARED => indicates if memory was freed.
    ;----------------------------------------------------------------------
    lda  SGC_MEMORY_CLEARED
    rts


;------------------------------------------------------------------------------
; Zero-page variables that store the "start" and "end" parameters.
;------------------------------------------------------------------------------
sgc_startNo: .byte $0
sgc_endNo:   .byte $0

.segment "BANKRAM0A"

;------------------------------------------------------------------------------
; bARunSpriteGarbageCollectorAll()
; Forces a GC run over the entire view table
;------------------------------------------------------------------------------
_bARunSpriteGarbageCollectorAll:
    lda VIEW_TABLE_SIZE - 1
    sta sgc_endNo
    lda #$0
    sta sgc_endNo
    jmp runSpriteGarbageCollectorAsmStart

;------------------------------------------------------------------------------
; bADeallocSpriteMemory
;
; PURPOSE:
;   Frees (deallocates) a single sprite memory entry in `_spriteAllocTable`
;   and possibly adjusts the "walls" (ZP_PTR_WALL_32 / ZP_PTR_WALL_64)
;   or segments (ZP_PTR_SEG_32 / ZP_PTR_SEG_64) to reclaim space.
;
; INPUT REGISTERS:
;   X = The high byte used to decide which portion of the sprite table to free.
;       In some calling conventions, X is also used to store the table index.
;
; PROCESS OVERVIEW:
;   1) Check if X == 0 => means "high byte not set." If not zero => "high set."
;   2) Look up the final index in `_bASpriteAddressReverseHighSet` or
;      `_bASpriteAddressReverseHighNotSet` using Y as the sub-index. This is
;      how we map the pointer’s high byte back to the correct table index.
;   3) Clear (`stz`) the relevant entry in `_spriteAllocTable,x`.
;   4) Decide if we need to push back the "32K wall" or "64K wall" based on X
;      and other references:
;       - If near the top => treat as 32K region.
;       - Otherwise => might need to push the 64K wall, or adjust the 64K segment.
;   5) Update or restore the zero-page pointers:
;       - `ZP_PTR_WALL_32`, `ZP_PTR_WALL_64` => track the top of used space
;       - `ZP_PTR_SEG_32`, `ZP_PTR_SEG_64` => track the next segment boundary
;   6) Return.
;
; NOTES:
;   - "Walls" typically represent high water marks for used memory,
;     while "segments" track the next available chunk within those walls.
;   - #SPRITE_ALLOC_TABLE_SIZE, #SEGMENT_LARGE_SPACES, SPRITE_START >> 8,
;     and other constants must be defined elsewhere in your code.
;------------------------------------------------------------------------------
bADeallocSpriteMemory:
    ;----------------------------------------------------------------------
    ; Step 1) Check if X == 0 => "high byte not set"
    ;         If X != 0 => "high byte set"
    ;         This helps us pick which reverse-lookup table to use below.
    ;----------------------------------------------------------------------
    cpx #$0
    beq @highNotSet           ; If X == 0 => jump to @highNotSet

@highSet:
    ;----------------------------------------------------------------------
    ;  If X != 0, we interpret the pointer’s high byte as "set."
    ;  TAY => copy X into Y (to index a table),
    ;  then LDX => load from _bASpriteAddressReverseHighSet,y.
    ;----------------------------------------------------------------------
    tay
    ldx _bASpriteAddressReverseHighSet,y
    bra @clearEntry           ; Jump to clearing the table entry

@highNotSet:
    ;----------------------------------------------------------------------
    ;  If X == 0, we interpret the pointer’s high byte as "not set."
    ;  We do "sec" + "sbc #SPRITE_START >> 8" => effectively adjusting
    ;  the pointer to align with the partial sprite address space.
    ;  Then TAY => use that result to index _bASpriteAddressReverseHighNotSet.
    ;----------------------------------------------------------------------
    sec
    sbc #SPRITE_START >> 8    ; Adjust X to reflect the "low region"
    tay
    ldx _bASpriteAddressReverseHighNotSet,y

;----------------------------------------------------------------------
; @clearEntry: The index we just loaded in X points to the sprite entry
;              in _spriteAllocTable that we must clear.
;----------------------------------------------------------------------
@clearEntry:
    stz _spriteAllocTable,x   ; Clear the allocation entry => effectively "free" it

    ;----------------------------------------------------------------------
    ; Step 4) Decide if we need to push back the 32K wall or 64K wall, or
    ;         adjust segments. We do this by comparing X to specific
    ;         boundaries in the table.
    ;----------------------------------------------------------------------
    cpx #SPRITE_ALLOC_TABLE_SIZE - SEGMENT_LARGE_SPACES
    bcs @pushBackWall32       ; If X >= that threshold => treat as 32K allocation

    cpx ZP_PTR_WALL_64
    bcs @pushBackWall64       ; If X >= ZP_PTR_WALL_64 => might need to push 64K wall

@pushBackWall32:
    ;----------------------------------------------------------------------
    ; Freed an entry in the 32K region => possibly push back "wall_32"
    ;----------------------------------------------------------------------
    txa
    beq @end                  ; If X == 0 => no need to push back anything

    ; If not zero, compare with ZP_PTR_WALL_32
    cpx ZP_PTR_WALL_32
    bne @reduceSegment32      ; If different => we skip adjusting the wall
    dec ZP_PTR_WALL_32        ; If the index equals the wall => push the wall back by 1

@reduceSegment32:
    ;----------------------------------------------------------------------
    ; Possibly adjust the segment pointer (ZP_PTR_SEG_32) if the
    ; newly freed index is less than the current segment pointer.
    ;----------------------------------------------------------------------
    cpx ZP_PTR_SEG_32
    bcs @end                  ; If X >= segment => nothing to do
    stx ZP_PTR_SEG_32         ; Move segment pointer down to X
    bra @end

@pushBackWall64:
    ;----------------------------------------------------------------------
    ; Freed an entry in the 64K region => possibly push back "wall_64"
    ;----------------------------------------------------------------------
    cpx #SPRITE_ALLOC_TABLE_SIZE - SEGMENT_LARGE_SPACES
    beq @reduceSegment64      ; If exactly at the edge => skip pushing wall
    cpx ZP_PTR_WALL_64
    bne @reduceSegment64      ; If X != wall => skip adjusting the wall

    ; If X == ZP_PTR_WALL_64, we can push that wall by SEGMENT_LARGE_SPACES
    clc
    lda ZP_PTR_WALL_64
    adc #SEGMENT_LARGE_SPACES
    sta ZP_PTR_WALL_64

@reduceSegment64:
    ;----------------------------------------------------------------------
    ; Possibly adjust ZP_PTR_SEG_64 if X is below (or equal to) the current
    ; segment pointer for the 64K region. That means we freed a chunk in
    ; the middle of an allocated block and want to reclaim it.
    ;----------------------------------------------------------------------
    cpx ZP_PTR_SEG_64
    bcc @end                  ; If X < segment => do nothing
    beq @end                  ; If X == segment => do nothing, either
    stx ZP_PTR_SEG_64         ; If X > segment => bring segment pointer back down

@end:
    rts                        ; Return => done deallocating and adjusting walls


; Reverse lookup arrays:
_bASpriteAddressReverseHighNotSet: .res 22, $0
_bASpriteAddressReverseHighSet:    .res $F8, $0

.endif