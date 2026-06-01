#pragma once
#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_


/* Commander X16 PETSCII / GETKEY codes */
#define KEY_ESC         0x1B
#define KEY_TAB         0x09
#define KEY_ENTER       0x0D
#define KEY_BACKSPACE   0x14
#define KEY_DELETE      0x14
#define KEY_INSERT      0x94
#define KEY_HOME        0x13
#define KEY_END         0x04
#define KEY_PGUP        0x82
#define KEY_PGDN        0x02

#define KEY_UP          0x91
#define KEY_DOWN        0x11
#define KEY_LEFT        0x9D
#define KEY_RIGHT       0x1D

#define KEY_F1          0x85
#define KEY_F2          0x89
#define KEY_F3          0x86
#define KEY_F4          0x8A
#define KEY_F5          0x87
#define KEY_F6          0x8B
#define KEY_F7          0x88
#define KEY_F8          0x8C
#define KEY_F9          0x01
#define KEY_F10         0x06
#define KEY_F11         0x07
#define KEY_F12         0x08

#define KEY_SPACE       0x20
#define KEY_0           0x30
#define KEY_1           0x31
#define KEY_9           0x39

#define KEY_CAP_A       0xC1
#define KEY_LOWER_A     0x41
#define KEY_CAP_Z       0xDA
#define KEY_LOWER_Z     0x5A

#define ASCII_DIFF      0x20

#define AGI_ESC_ASCII      0x1B
#define AGI_ESC_SCAN       0

#define AGI_TAB_ASCII      0x09
#define AGI_TAB_SCAN       0

#define AGI_ENTER_ASCII    0x0D
#define AGI_ENTER_SCAN     0

#define AGI_BACKSPACE_ASCII 0x08
#define AGI_BACKSPACE_SCAN  0

#define AGI_SPACE_ASCII    0x20
#define AGI_SPACE_SCAN     0

#define AGI_A_ASCII        0x41   /* Uppercase A */
#define AGI_A_SCAN         0
#define AGI_a_ASCII        0x61   /* Lowercase a */
#define AGI_a_SCAN         0

#define AGI_1_ASCII        0x31
#define AGI_1_SCAN         0
#define AGI_9_ASCII        0x39
#define AGI_9_SCAN         0

/* ------------------- Extended keys (Scan-code way) ------------------- */
#define AGI_F1_ASCII       0
#define AGI_F1_SCAN        59
#define AGI_F2_ASCII       0
#define AGI_F2_SCAN        60
#define AGI_F3_ASCII       0
#define AGI_F3_SCAN        61
#define AGI_F4_ASCII       0
#define AGI_F4_SCAN        62
#define AGI_F5_ASCII       0
#define AGI_F5_SCAN        63     /* Classic Save */
#define AGI_F6_ASCII       0
#define AGI_F6_SCAN        64
#define AGI_F7_ASCII       0
#define AGI_F7_SCAN        65     /* Classic Restore */
#define AGI_F8_ASCII       0
#define AGI_F8_SCAN        66
#define AGI_F9_ASCII       0
#define AGI_F9_SCAN        67
#define AGI_F10_ASCII      0
#define AGI_F10_SCAN       68

#define AGI_UP_ASCII       0
#define AGI_UP_SCAN        72
#define AGI_DOWN_ASCII     0
#define AGI_DOWN_SCAN      80
#define AGI_LEFT_ASCII     0
#define AGI_LEFT_SCAN      75
#define AGI_RIGHT_ASCII    0
#define AGI_RIGHT_SCAN     77

#define AGI_HOME_ASCII     0
#define AGI_HOME_SCAN      71
#define AGI_END_ASCII      0
#define AGI_END_SCAN       79
#define AGI_PGUP_ASCII     0
#define AGI_PGUP_SCAN      73
#define AGI_PGDN_ASCII     0
#define AGI_PGDN_SCAN      81
#define AGI_INSERT_ASCII   0
#define AGI_INSERT_SCAN    82
#define AGI_DELETE_ASCII   0
#define AGI_DELETE_SCAN    83

#endif
