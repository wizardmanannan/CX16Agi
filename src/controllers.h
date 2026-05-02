#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "general.h"
#include "memoryManager.h"
#include "keyboard.h"

#define NO_CONTROLLERS 256
#define NO_ASSOCIATED 0xFF
#define LARGEST_PETSCII 0xDA
#define NO_CONTROLLER_BITS NO_CONTROLLERS / 8

#pragma wrapped-call (push, trampoline, CONTROLLER_BANK)
void b1ResetControllers();
void b1AssociateController(byte asciiCode, byte scanCode, byte controller);
boolean b1SetController(byte petscii);
byte b1IsControllerSet(byte controller);
#pragma wrapped-call (pop)

#endif