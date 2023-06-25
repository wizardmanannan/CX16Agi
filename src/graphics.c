#include "graphics.h"

int* dcVideo = (int*)0x9F29;

#pragma code-name (push, "BANKRAM07");
void b7InitGraphics()
{
	*(dcVideo) = 0b00010001;
}
#pragma code-name (pop);
