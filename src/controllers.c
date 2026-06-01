#include "controllers.h"

#pragma bss-name (push, "BANKRAM01")
byte b1Controllers[NO_CONTROLLERS];
byte b1PetciiToControllers[LARGEST_PETSCII + 1];
byte b1ControllerBits[NO_CONTROLLER_BITS];
#pragma bss-name (pop)

#pragma code-name (push, "BANKRAM01")
void b1ResetControllers()
{
    memset(b1ControllerBits, 0, NO_CONTROLLER_BITS);
}

void b1InitControllers()
{
    memset(b1Controllers, NOT_ASSOCIATED, NO_CONTROLLERS);
    memset(b1PetciiToControllers, NOT_ASSOCIATED, LARGEST_PETSCII + 1);
    memset(b1ControllerBits, 0, NO_CONTROLLER_BITS);
}

void b1AssociateController(byte asciiCode, byte scanCode, byte controller)
{
    byte petscii = NOT_ASSOCIATED;

    /* ------------------- ASCII keys (scanCode == 0) ------------------- */
    if (scanCode == 0)
    {
        switch (asciiCode)
        {
            /* Special controls */
            case 0x1B:  petscii = KEY_ESC;        break;
            case 0x09:  petscii = KEY_TAB;        break;
            case 0x0D:  petscii = KEY_ENTER;      break;
            case 0x08:  petscii = KEY_BACKSPACE;  break;
            case 0x20:  petscii = KEY_SPACE;      break;

            /* Uppercase letters A–Z */
            case 0x41:  petscii = KEY_CAP_A;  break;  /* A */
            case 0x42:  petscii = 0xC2; break; /* B */
            case 0x43:  petscii = 0xC3; break; /* C */
            case 0x44:  petscii = 0xC4; break; /* D */
            case 0x45:  petscii = 0xC5; break; /* E */
            case 0x46:  petscii = 0xC6; break; /* F */
            case 0x47:  petscii = 0xC7; break; /* G */
            case 0x48:  petscii = 0xC8; break; /* H */
            case 0x49:  petscii = 0xC9; break; /* I */
            case 0x4A:  petscii = 0xCA; break; /* J */
            case 0x4B:  petscii = 0xCB; break; /* K */
            case 0x4C:  petscii = 0xCC; break; /* L */
            case 0x4D:  petscii = 0xCD; break; /* M */
            case 0x4E:  petscii = 0xCE; break; /* N */
            case 0x4F:  petscii = 0xCF; break; /* O */
            case 0x50:  petscii = 0xD0; break; /* P */
            case 0x51:  petscii = 0xD1; break; /* Q */
            case 0x52:  petscii = 0xD2; break; /* R */
            case 0x53:  petscii = 0xD3; break; /* S */
            case 0x54:  petscii = 0xD4; break; /* T */
            case 0x55:  petscii = 0xD5; break; /* U */
            case 0x56:  petscii = 0xD6; break; /* V */
            case 0x57:  petscii = 0xD7; break; /* W */
            case 0x58:  petscii = 0xD8; break; /* X */
            case 0x59:  petscii = 0xD9; break; /* Y */
            case 0x5A:  petscii = KEY_CAP_Z;  break;  /* Z */

            /* Lowercase letters a–z */
            case 0x61:  petscii = KEY_LOWER_A; break; /* a */
            case 0x62:  petscii = 0xC2 + ASCII_DIFF; break;
            case 0x63:  petscii = 0xC3 + ASCII_DIFF; break;
            case 0x64:  petscii = 0xC4 + ASCII_DIFF; break;
            case 0x65:  petscii = 0xC5 + ASCII_DIFF; break;
            case 0x66:  petscii = 0xC6 + ASCII_DIFF; break;
            case 0x67:  petscii = 0xC7 + ASCII_DIFF; break;
            case 0x68:  petscii = 0xC8 + ASCII_DIFF; break;
            case 0x69:  petscii = 0xC9 + ASCII_DIFF; break;
            case 0x6A:  petscii = 0xCA + ASCII_DIFF; break;
            case 0x6B:  petscii = 0xCB + ASCII_DIFF; break;
            case 0x6C:  petscii = 0xCC + ASCII_DIFF; break;
            case 0x6D:  petscii = 0xCD + ASCII_DIFF; break;
            case 0x6E:  petscii = 0xCE + ASCII_DIFF; break;
            case 0x6F:  petscii = 0xCF + ASCII_DIFF; break;
            case 0x70:  petscii = 0xD0 + ASCII_DIFF; break;
            case 0x71:  petscii = 0xD1 + ASCII_DIFF; break;
            case 0x72:  petscii = 0xD2 + ASCII_DIFF; break;
            case 0x73:  petscii = 0xD3 + ASCII_DIFF; break;
            case 0x74:  petscii = 0xD4 + ASCII_DIFF; break;
            case 0x75:  petscii = 0xD5 + ASCII_DIFF; break;
            case 0x76:  petscii = 0xD6 + ASCII_DIFF; break;
            case 0x77:  petscii = 0xD7 + ASCII_DIFF; break;
            case 0x78:  petscii = 0xD8 + ASCII_DIFF; break;
            case 0x79:  petscii = 0xD9 + ASCII_DIFF; break;
            case 0x7A:  petscii = KEY_LOWER_Z; break; /* z */

            /* Digits */
            case 0x30:  petscii = KEY_0; break;
            case 0x31:  petscii = KEY_1; break;
            case 0x32:  petscii = 0x32; break;
            case 0x33:  petscii = 0x33; break;
            case 0x34:  petscii = 0x34; break;
            case 0x35:  petscii = 0x35; break;
            case 0x36:  petscii = 0x36; break;
            case 0x37:  petscii = 0x37; break;
            case 0x38:  petscii = 0x38; break;
            case 0x39:  petscii = KEY_9; break;

            /* Common punctuation */
            case 0x21:  petscii = 0x21; break; /* ! */
            case 0x22:  petscii = 0x22; break; /* " */
            case 0x23:  petscii = 0x23; break; /* # */
            case 0x24:  petscii = 0x24; break; /* $ */
            case 0x25:  petscii = 0x25; break; /* % */
            case 0x26:  petscii = 0x26; break; /* & */
            case 0x27:  petscii = 0x27; break; /* ' */
            case 0x28:  petscii = 0x28; break; /* ( */
            case 0x29:  petscii = 0x29; break; /* ) */
            case 0x2A:  petscii = 0x2A; break; /* * */
            case 0x2B:  petscii = 0x2B; break; /* + */
            case 0x2C:  petscii = 0x2C; break; /* , */
            case 0x2D:  petscii = 0x2D; break; /* - */
            case 0x2E:  petscii = 0x2E; break; /* . */
            case 0x2F:  petscii = 0x2F; break; /* / */
            case 0x3A:  petscii = 0x3A; break; /* : */
            case 0x3B:  petscii = 0x3B; break; /* ; */
            case 0x3C:  petscii = 0x3C; break; /* < */
            case 0x3D:  petscii = 0x3D; break; /* = */
            case 0x3E:  petscii = 0x3E; break; /* > */
            case 0x3F:  petscii = 0x3F; break; /* ? */
            case 0x40:  petscii = 0x40; break; /* @ */

            default: break;
        }
    }
    /* ------------------- Scan-code keys (asciiCode == 0) ------------------- */
    else if (asciiCode == 0)
    {
        switch (scanCode)
        {
            case 59: petscii = KEY_F1;     break;
            case 60: petscii = KEY_F2;     break;
            case 61: petscii = KEY_F3;     break;
            case 62: petscii = KEY_F4;     break;
            case 63: petscii = KEY_F5;     break;
            case 64: petscii = KEY_F6;     break;
            case 65: petscii = KEY_F7;     break;
            case 66: petscii = KEY_F8;     break;
            case 67: petscii = KEY_F9;     break;
            case 68: petscii = KEY_F10;    break;

            case 72: petscii = KEY_UP;     break;
            case 80: petscii = KEY_DOWN;   break;
            case 75: petscii = KEY_LEFT;   break;
            case 77: petscii = KEY_RIGHT;  break;

            case 71: petscii = KEY_HOME;   break;
            case 79: petscii = KEY_END;    break;
            case 73: petscii = KEY_PGUP;   break;
            case 81: petscii = KEY_PGDN;   break;
            case 82: petscii = KEY_INSERT; break;
            case 83: petscii = KEY_DELETE; break;

            default: break;
        }
    }

    if (petscii < NOT_ASSOCIATED)
    {
        b1Controllers[controller] = petscii;        
        b1PetciiToControllers[petscii] = controller;
    }
}

boolean b1SetControllerByPetscii(byte petscii)
{
    byte controller = b1PetciiToControllers[petscii];

    return b1SetController(controller);
}

boolean b1SetController(byte controller)
{
    if (controller < NOT_ASSOCIATED)
    {
        b1ControllerBits[controller >> 3] |= (1 << (controller & 7));
        return TRUE;
    }

    return FALSE;
}

#pragma code-name (pop)