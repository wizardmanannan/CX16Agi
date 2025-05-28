.ifndef SOUND_INC
SOUND_INC = 1

.import _b1LoadedSoundsPointer

.segment "ZEROPAGE"
_ZP_CURRENTLY_PLAYING_NOTE_1: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_2: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_3: .word $0
_ZP_CURRENTLY_PLAYING_NOTE_NOISE: .word $0

.segment "BANKRAM01"
_b1Ch1Ticks: .word $0
_b1Ch2Ticks: .word $0
_b1Ch3Ticks: .word $0
_b1Ch4Ticks: .word $0

_b1IsPlaying: .byte $0



.segment "CODE"



.endif