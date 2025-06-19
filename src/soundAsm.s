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
NOISE_CHANNEL = 3

DEFAULT_VOLUME = $C0 ;Volume of zero with left and right bits set
WAVE_FORM_PULSE_WIDTH = $3F ;Default wave form and 50% duty
NOISE_WAVE = $C0
SAW_TOOTH = $40

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

LATCH_TO_CH2 = $0

.segment "BANKRAM01"
b1SoundHandler:
.scope
start:
SET_VERA_ADDRESS_IMMEDIATE FIRST_PSG_VOL_REGISTER, #$0, #1

ldx #$0
channelLoop:
txa
lsr
tay
lda _b1IsPlaying,y
bne checkNoteLength
jmp silenceChannel
checkNoteLength:
ldy #$1
lda _b1Ch1Ticks + 1,x
bne deductOne
lda _b1Ch1Ticks,x
bne deductOne
jmp zeroDurationNote

deductOne:
lda VERA_data0
lda VERA_data0
lda VERA_data0
lda VERA_data0

dec _b1Ch1Ticks,x
lda _b1Ch1Ticks,x
cmp #$FF
bne incrementChannelCounter
jmp durationDeductHigh

playNote:
store:
lda VERA_addr_bank
ora #$8
sta VERA_addr_bank
stz VERA_data0
lda VERA_data0

lda VERA_addr_bank
and #$F7
sta VERA_addr_bank

jmp readSound

zeroVolume:
stz VERA_data0
bra waveForm

nonZeroVolume:
ora #DEFAULT_VOLUME
sta VERA_data0

waveForm:
cpx #NOISE_CHANNEL * 2
bcc sawTooth

noiseWave:
lda #SAW_TOOTH
sta VERA_data0
bra goToNextNote

sawTooth:
lda #WAVE_FORM_PULSE_WIDTH
sta VERA_data0

goToNextNote:
lda VERA_data0
lda VERA_data0

incrementChannelCounter:
inx
inx
cpx #NO_AGI_CHANNELS * 2
beq end
jmp channelLoop

end:
rts
zeroDurationNote:
clc 
lda #NO_NOTES
adc _ZP_CURRENTLY_PLAYING_NOTE_1,x
sta _ZP_CURRENTLY_PLAYING_NOTE_1,x
bcc loadNote
highByte:
inc _ZP_CURRENTLY_PLAYING_NOTE_1 + 1,x

loadNote:
lda _ZP_CURRENTLY_PLAYING_NOTE_1,x
sta SOUND_SREG
lda _ZP_CURRENTLY_PLAYING_NOTE_1 + 1,x
sta SOUND_SREG + 1

jmp getTicksJump

storeTicks:
sta  ticks + 1
and  ticks
cmp #$FF
beq disableChannel

lda ticks
sta _b1Ch1Ticks,x
lda ticks + 1
sta  _b1Ch1Ticks + 1,x

jmp playNote

durationDeductHigh:
dec _b1Ch1Ticks + 1,x
bra incrementChannelCounter

disableChannel:
txa
lsr
tay
lda #$0
sta _b1IsPlaying,y

dec _b1ChannelsPlaying
bne silenceChannel

lda _b1EndSoundFlag
SET_FLAG_NON_INTERPRETER SOUND_SREG

silenceChannel:
lda #$0
;Freq
sta VERA_data0
sta VERA_data0

;Vol
sta VERA_data0

;WaveForm/Width
sta VERA_data0

jmp incrementChannelCounter

b1CopyChannel2:
sec
lda VERA_addr_low
sbc #NO_BYTES_PER_CHANNEL
tay
lda VERA_addr_high
sbc #$0
inc VERA_ctrl
sty VERA_addr_low
sta VERA_addr_high
lda #$11
sta VERA_addr_bank

lda VERA_data1
sta VERA_data0
lda VERA_data1
sta VERA_data0

dec VERA_ctrl

jmp returnCopyChannel    

.segment "CODE"
ticks: .word $0
readSound:
lda _b1SoundDataBank
sta RAM_BANK

ldy #FREQUENCY_BYTE
lda (SOUND_SREG),y
bne playFrequency
cpx #NOISE_CHANNEL * 2
bcc playFrequency

lda #SOUND_BANK
sta RAM_BANK
jmp b1CopyChannel2
returnCopyChannel:
lda _b1SoundDataBank 
sta RAM_BANK
bra setVolume

playFrequency:
sta VERA_data0
ldy #FREQUENCY_BYTE + 1
lda (SOUND_SREG),y
sta VERA_data0

setVolume:
ldy #VOLUME_BYTE 
lda (SOUND_SREG),y

ldy #SOUND_BANK
sty RAM_BANK

cmp #$0
beq @zeroVolumeJump
jmp nonZeroVolume
@zeroVolumeJump:
jmp zeroVolume

getTicksJump:
lda _b1SoundDataBank
sta RAM_BANK

lda (SOUND_SREG)
sta ticks
ldy #$1
lda (SOUND_SREG),y

ldy #SOUND_BANK
sty RAM_BANK
jmp storeTicks

.endscope

.segment "BANKRAM01"
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