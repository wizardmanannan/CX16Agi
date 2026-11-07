/* Host tests replace inline hardware instructions, retaining the C display path. */
void native_asm(const char* instruction, ...);
#define asm(...) native_asm(__VA_ARGS__)

#ifndef __fastcall__
#define __fastcall__
#endif
