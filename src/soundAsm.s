.ifndef SOUND_INC
SOUND_INC = 1

.include "globalGraphics.s"          ; Include global graphics definitions
.include "global.s"                  ; Include global definitions

NO_AGI_CHANNELS = 4                 ; Number of AGI sound channels
NO_NOTES = 5                       ; Number of notes in a sequence
END_MARKER = $FFFF                 ; End marker for sound data
FREQUENCY_BYTE = 2                 ; Offset for frequency byte in note data
VOLUME_BYTE = 4                    ; Offset for volume byte in note data
NO_SYSTEM_CHANNELS = 16            ; Number of system sound channels
NO_BYTES_PER_CHANNEL = 4           ; Number of bytes per channel
NOISE_CHANNEL = 3                  ; Channel index for noise channel

DEFAULT_VOLUME = $C0               ; Volume of zero with left and right bits set
WAVE_FORM_PULSE_WIDTH = $3F       ; Default wave form and 50% pulse width duty cycle
NOISE_WAVE = $C0                  ; Noise waveform parameter
SAW_TOOTH = $40                   ; Saw tooth waveform parameter

.import _bBLoadedSoundsPointer       ; Import pointer for loaded sounds

.segment "ZEROPAGE"
; Zero page variables for currently playing notes for each channel
_ZP_CURRENTLY_PLAYING_NOTE_1: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_2: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_3: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_NOISE: .word $0

SOUND_SREG: .word $0               ; Sound data register

.segment "BANKRAM0B"
; Variables for sound playback timing and state
_bBCh1Ticks: .word $0              ; Remaining ticks for Channel 1
_bBCh2Ticks: .word $0              ; Remaining ticks for Channel 2
_bBCh3Ticks: .word $0              ; Remaining ticks for Channel 3
_bBCh4Ticks: .word $0              ; Remaining ticks for Channel 4

_bBChannelsPlaying: .byte $0       ; Number of channels currently playing
_bBEndSoundFlag: .byte $0          ; Flag indicating end of sound playback
_bBSoundDataBank: .byte $0         ; Bank containing sound data
_bBIsPlaying: .res NO_AGI_CHANNELS ; Flags for channels currently playing

LATCH_TO_CH2 = $0                  ; Latch value used in some noise handling

; Frequencies used for noise channel sound effects (in approximate timer counts)
bBNoiseFreq:
    .word 1604     ; ~2230 Hz noise frequency
    .word 3209     ; ~1115 Hz noise frequency
    .word 6423     ; ~557 Hz noise frequency

.segment "BANKRAM0B"
;Note X is the channel index, and it is counted in 2s
bBSoundHandler:
.scope
start:
    SET_VERA_ADDRESS_IMMEDIATE FIRST_PSG_VOL_REGISTER, #$0, #1  ; Set VERA PSG volume register address

    ldx #$0                      ; Start loop counter at 0

channelLoop:
    txa                         ; Transfer X to A
    lsr                         ; Logical shift right A (divide X by 2).
    tay                         ; Transfer A to Y (indexing _bBIsPlaying bits)
    lda _bBIsPlaying,y           ; Load _bBIsPlaying flag for current channel (Y)
    bne checkNoteLength          ; If channel is playing, check note length
    jmp silenceChannel           ; Otherwise silence the channel

checkNoteLength:
    ldy #$1                      ; Offset 1 into ticks (high byte)
    lda _bBCh1Ticks + 1,x        ; Load high byte of channel ticks
    bne deductOne                ; If non-zero, deduct one tick
    lda _bBCh1Ticks,x            ; Load low byte of channel ticks
    bne deductOne                ; If non-zero, deduct one tick
    jmp zeroDurationNote         ; Zero ticks means load next note

deductOne:
    lda VERA_data0               ; Move to the volume byte 
    lda VERA_data0               ;    
    lda VERA_data0
    lda VERA_data0

    dec _bBCh1Ticks,x            ; Decrement low byte ticks
    lda _bBCh1Ticks,x            ; Reload low byte ticks
    cmp #$FF                    ; Check if underflow (went below zero)
    bne incrementChannelCounter  ; If not, continue to next channel
    jmp durationDeductHigh       ; Otherwise decrement high byte ticks

playNote:
store:
    lda VERA_addr_bank
    ora #$8
    sta VERA_addr_bank           ; Turn on backwards so that after we set volume to zero we return to frequency

    stz VERA_data0               ; Set volume to zero to reduce chance of squeak
    lda VERA_data0               

    lda VERA_addr_bank
    and #$F7
    sta VERA_addr_bank           ; Clear bit previously set

    jmp readSound                ; Jump to sound data read routine

zeroVolume:
    stz VERA_data0               ; Set zero volume
    bra waveForm

nonZeroVolume:
    ora #DEFAULT_VOLUME          ; OR with default volume bits
    sta VERA_data0

waveForm:
    cpx #NOISE_CHANNEL * 2       ; Compare channel with noise channel index * 2
    bcc squareWave               ; If less, jump to square waveform

    lda isWhiteNoise             ; Load noise flag
    beq squareWave               ; If zero, jump to square waveform

noiseWave:
    lda #NOISE_WAVE              ; Load noise waveform value
    sta VERA_data0              ; Store in waveform register
    bra goToNextNote

squareWave:
    lda #WAVE_FORM_PULSE_WIDTH   ; Load square wave pulse width value
    sta VERA_data0

goToNextNote:
    lda VERA_data0              ; Dummy loads for timing and synchronization
    lda VERA_data0

incrementChannelCounter:
    inx                         ; Increment low byte index (X)
    inx                         ; Increment high byte index (X)
    cpx #NO_AGI_CHANNELS * 2    ; Check if all AGI channels processed
    beq end                     ; If yes, end routine
    jmp channelLoop             ; Otherwise continue loop

end:
    rts                         ; Return from subroutine

zeroDurationNote:
    clc                         ; Clear carry before addition
    lda #NO_NOTES               ; Load number of notes
    adc _ZP_CURRENTLY_PLAYING_NOTE_1,x  ; Add current note index
    sta _ZP_CURRENTLY_PLAYING_NOTE_1,x  ; Store updated note index
    bcc loadNote                ; If no overflow, load note

highByte:
    inc _ZP_CURRENTLY_PLAYING_NOTE_1 + 1,x  ; Increment high byte if overflow

loadNote:
    lda _ZP_CURRENTLY_PLAYING_NOTE_1,x      ; Load current note low byte
    sta SOUND_SREG                           ; Store in sound register low byte
    lda _ZP_CURRENTLY_PLAYING_NOTE_1 + 1,x  ; Load high byte
    sta SOUND_SREG + 1                       ; Store in sound register high byte

    jmp getTicksJump           ; Jump to get ticks for the note

storeTicks:
    sta ticks + 1              ; Store high byte of ticks
    and ticks                 ; AND low byte with accumulator
    cmp #$FF                  ;  If both ticks are FF disable channel
    beq disableChannel         ; If match, disable channel

    lda ticks                 ; Load low byte of ticks
    sta _bBCh1Ticks,x          ; Store ticks low byte for channel
    lda ticks + 1              ; Load high byte of ticks
    sta _bBCh1Ticks + 1,x      ; Store ticks high byte

    jmp playNote              ; Play new note

durationDeductHigh:
    dec _bBCh1Ticks + 1,x      ; Decrement high byte of ticks for channel
    bra incrementChannelCounter; Branch to increment channel counter

disableChannel:
    txa                       ; Transfer X to A
    lsr                       ; Logical shift right A
    tay                       ; Transfer A to Y 
    lda #$0                   ; Load zero
    sta _bBIsPlaying,y         ; Clear playing flag for channel

    dec _bBChannelsPlaying     ; Decrement count of channels playing
    bne silenceChannel         ; If still playing more channels, silence

    lda _bBEndSoundFlag        ; Load end of sound flag
    SET_FLAG_NON_INTERPRETER SOUND_SREG   ; Set flag for non-interpreter sound register

silenceChannel:
    lda #$0                   ; Load zero
; Silence frequency
    sta VERA_data0            ; Write zero frequency low byte
    sta VERA_data0            ; Write zero frequency high byte

; Silence volume
    sta VERA_data0            ; Set volume to zero

; Silence waveform/width
    sta VERA_data0            ; Set waveform to zero

    jmp incrementChannelCounter ; Continue to next channel

bBPlayNoise:
    ldy #$1                   ; Load 1 into Y (noise channel index)
    sty isWhiteNoise          ; Set white noise flag

determineIfPredefinedOrLatched:
    and #$3                   ; The last two bits determine whether this noise is predefined or latched. 3, means latched, everything else predefined
    cmp #$3                   ; 
    bne bBPlayPredefinedNoise  ; If not equal, play predefined noise

bBCopyChannel2:
    sec                       
    lda VERA_addr_low          ;Set VERA ch1 to the current value of channel 2, by deducting the number of notes from channel zero and going back a channel
    sbc #NO_BYTES_PER_CHANNEL ;Step 1 Deduct
    tay                       
    lda VERA_addr_high         
    sbc #$0                   
    inc VERA_ctrl    

    sty VERA_addr_low       ;Step 2, Store In Ch1 Register   
    sta VERA_addr_high         
    lda #$11                  
    sta VERA_addr_bank

    lda VERA_data1            
    sta VERA_data0             ;Step 3: Store frequency
    lda VERA_data1            
    sta VERA_data0             

    dec VERA_ctrl              ;Return VERA_ctrl back to ch9

    jmp returnCopyChannel      ; Return

bBPlayPredefinedNoise:
    asl                       ; Shift accumulator left (multiply by 2)
    tay                       ; Store result in Y

    lda bBNoiseFreq,y          ; Load low byte of noise frequency from table
    sta VERA_data0             ; Store low byte frequency

    iny                       ; Increment Y to get high byte
    lda bBNoiseFreq,y          ; Load high byte noise frequency
    sta VERA_data0             ; Store high byte frequency

    jmp returnCopyChannel      ; Return from noise play

.segment "CODE"
isWhiteNoise: .byte $0            ; Flag indicating if noise is white noise
ticks: .word $0                   ; Current sound tick counter
volByte: .byte $0                ; Current volume byte

readSound:
    lda _bBSoundDataBank         ; Load sound data bank
    sta RAM_BANK                 ; Select RAM bank

    cpx #NOISE_CHANNEL * 2       ; Compare channel index with noise channel * 2
    bcc playFrequency            ; If channel < noise channel, play frequency

    stz isWhiteNoise             ; Set white noise flag to 0

    ldy #VOLUME_BYTE
    lda (SOUND_SREG),y           ; Load volume byte from sound register
    sta volByte                  ; Store volume in volByte
    bit volByte                  ; C code sets bit 7 to 1, if periodic
    bmi playFrequency            ; If N is set, the noise has already being computed, play as a frequency.

    ldy #FREQUENCY_BYTE + 1  ;This means white noise, we compute what we play here
    lda (SOUND_SREG),y           ; Load frequency high byte

    ldy #SOUND_BANK
    sty RAM_BANK                 ; Select sound bank RAM

    jmp bBPlayNoise              ; Play noise sound

returnCopyChannel:
    lda _bBSoundDataBank          ; Reload bank
    sta RAM_BANK                  ; Set RAM bank

    bra setVolume                 ; Jump to volume setting

returnToPlayFrequency:
    lda _bBSoundDataBank
    sta RAM_BANK

playFrequency:
    ldy #FREQUENCY_BYTE
    lda (SOUND_SREG),y            ; Load frequency low byte
    sta VERA_data0
    ldy #FREQUENCY_BYTE + 1
    lda (SOUND_SREG),y            ; Load frequency high byte
    sta VERA_data0

setVolume:
    ldy #VOLUME_BYTE 
    lda (SOUND_SREG),y            ; Load volume byte
    and #$7F                      ; Mask out top bit

     ldy #SOUND_BANK
    sty RAM_BANK                  ; Set bank for sound

    cmp #$0                       ; Compare volume with zero
    beq @zeroVolumeJump           ; If zero, jump to zero volume code
    jmp nonZeroVolume             ; Else jump to non-zero volume
@zeroVolumeJump:
    jmp zeroVolume

getTicksJump:
    lda _bBSoundDataBank
    sta RAM_BANK

    lda (SOUND_SREG)
    sta ticks                    ; Store low byte of ticks

    ldy #$1
    lda (SOUND_SREG),y
    ; fall through

    ldy #SOUND_BANK
    sty RAM_BANK
    jmp storeTicks               ; Store ticks and handle

.endscope

.segment "BANKRAM0B"
; void bBPsgClear() - Clear all PSG registers to silence sound
_bBPsgClear:
    SET_VERA_ADDRESS_IMMEDIATE PSG_REGISTERS, #$0, #1    ; Set to PSG registers start
    ldx #NO_SYSTEM_CHANNELS * NO_BYTES_PER_CHANNEL       ; Initialize counter for all system channels

@clearLoop:
    stz VERA_data0              ; Store zero to current PSG register
@checkLoop:
    dex                        ; Decrement counter
    bne @clearLoop              ; Repeat until all are cleared
    rts                        ; Return from subroutine

.endif