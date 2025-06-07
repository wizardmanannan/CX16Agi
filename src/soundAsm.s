.ifndef SOUND_INC
SOUND_INC = 1

.include "globalGraphics.s"
.include "global.s"

NO_AGI_CHANNELS = 4
NO_NOTES = 5
END_MARKER = $FFFF
FREQUENCY_BYTE = 2
VOLUME_BYTE = 4
NO_SYSTEM_CHANNELS = 16
NO_BYTES_PER_CHANNEL = 4


DEFAULT_VOLUME = $C0 ;Volume of zero with left and right bits set
WAVE_FORM_PULSE_WIDTH = $3F ;Default wave form and 50% duty

.import _b1LoadedSoundsPointer

.segment "ZEROPAGE"
_ZP_CURRENTLY_PLAYING_NOTE_1: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_2: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_3: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_NOISE: .word $0
SOUND_SREG: .word $0


.segment "BANKRAM01"
_b1Ch1Ticks: .word $0
_b1Ch2Ticks: .word $0
_b1Ch3Ticks: .word $0
_b1Ch4Ticks: .word $0
_b1ChannelsPlaying: .byte $0
_b1EndSoundFlag: .byte $0

_b1SoundDataBank: .byte $0
_b1IsPlaying: .res NO_AGI_CHANNELS

.segment "CODE"

soundHandler:
lda RAM_BANK
pha

@start:
lda #$0
sta @channelCounter

SET_VERA_ADDRESS_IMMEDIATE PSG_REGISTERS, #$0, #1

ldx #$0
@channelLoop:
lda #SOUND_BANK
sta RAM_BANK
txa
lsr
tay
lda _b1IsPlaying,y

bne @checkNoteLength
jmp @silenceChannel
@checkNoteLength:
ldy #$1
lda _b1Ch1Ticks + 1,x
bne @deductOne
lda _b1Ch1Ticks,x
beq @zeroDurationNote

@deductOne:
dec _b1Ch1Ticks,x
lda _b1Ch1Ticks,x
cmp #$FF
beq @durationDeductHigh
bra @incrementChannelCounter

@playNote:
lda _b1SoundDataBank
sta RAM_BANK

ldy #FREQUENCY_BYTE
lda (SOUND_SREG),y
sta VERA_data0
ldy #FREQUENCY_BYTE + 1
lda (SOUND_SREG),y
sta VERA_data0

ldy #VOLUME_BYTE 
lda (SOUND_SREG),y
bne @nonZeroVolume

@zeroVolume:
stz VERA_data0
bra @waveForm

@nonZeroVolume:
ora #DEFAULT_VOLUME
sta VERA_data0

@waveForm:
lda #WAVE_FORM_PULSE_WIDTH
sta VERA_data0
@goToNextNote:

@incrementChannelCounter:
inx
inx
cpx #$2 ;#NO_AGI_CHANNELS * 2
bne @channelLoop

@end:
pla
sta RAM_BANK
rts
@zeroDurationNote:
lda _b1SoundDataBank
sta RAM_BANK

clc 
lda #NO_NOTES
adc _ZP_CURRENTLY_PLAYING_NOTE_1,x
sta _ZP_CURRENTLY_PLAYING_NOTE_1,x
bcc @loadNote
@highByte:
inc _ZP_CURRENTLY_PLAYING_NOTE_1 + 1,x

@loadNote:
lda _ZP_CURRENTLY_PLAYING_NOTE_1,x
sta SOUND_SREG
lda _ZP_CURRENTLY_PLAYING_NOTE_1 + 1,x
sta SOUND_SREG + 1
lda (SOUND_SREG)
sta @ticks
ldy #$1
lda (SOUND_SREG),y
sta  @ticks + 1
and  @ticks
cmp #$FF
beq @disableChannel

lda #SOUND_BANK
sta RAM_BANK
lda @ticks
sta _b1Ch1Ticks,x
lda @ticks + 1
sta  _b1Ch1Ticks + 1,x

bra @playNote

@durationDeductHigh:
dec _b1Ch1Ticks + 1,x
bra @incrementChannelCounter

@disableChannel:
lda #SOUND_BANK
sta RAM_BANK

txa
lsr
tay
lda #$0
sta _b1IsPlaying,y

dec _b1ChannelsPlaying
bne @silenceChannel

lda _b1EndSoundFlag
SET_FLAG_NON_INTERPRETER SOUND_SREG

@silenceChannel:
lda #$0
;Freq
sta VERA_data0
sta VERA_data0

;Vol
sta VERA_data0

;WaveForm/Width
sta VERA_data0

bra @incrementChannelCounter

@channelCounter: .byte $0
@noteByte: .byte $0
@ticks: .word $0

;void b1PsgClear()
_b1PsgClear:
SET_VERA_ADDRESS_IMMEDIATE PSG_REGISTERS, #$0, #1
ldx #NO_SYSTEM_CHANNELS * NO_BYTES_PER_CHANNEL
@clearLoop:
stz VERA_data0
@checkLoop:
dex
bne @clearLoop
rts
.endif