#include "picture.h"

#define CheckVRAMIsInited

boolean pictureUpdateReady = FALSE;

#pragma code-name (push, "BANKRAM11")
void initPicture()
{
    byte defaultByte = DEFAULT_COLOR << 2 + DEFAULT_COLOR;
    asm("sei"); //Disable while we are in the middle of updating the buffers
    
    asm("cli");
}
#pragma code-name (pop)

////Warning call from IRQ Handler Only
//void initPictureVRAM()
//{
//
////    byte l0TileAddress = 0x0;
////    int i;
////    
//#ifdef  CheckVRAMIsInited
//    printf("Init Picture VRAM Called");
//#endif //  CheckVRAMIsInited
////
////
////    for (i = 0; i < (BITMAP_WIDTH * BITMAP_HEIGHT) / 2; i++)
////    {
////        vpoke(0x000, i);
////    }
//}

