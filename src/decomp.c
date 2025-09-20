///***************************************************************************
//** decomp.c
//**
//** Routines that deal with AGI version 3 specific features.
//** The original LZW code is from DJJ, October 1989, p.86.
//** It has been modified to handle AGI compression.
//**
//** (c) 1997  Lance Ewing
//***************************************************************************/
//
//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
//
//
//#include "decomp.h"
//
//#define MAXBITS 12
//#define TABLE_SIZE 18041
//#define START_BITS 9
//
//#ifndef TRUE
//#define TRUE 1
//#endif
//#ifndef FALSE
//#define FALSE 0
//#endif
//
//int BITS, MAX_VALUE, MAX_CODE;
//unsigned int *prefix_code;
//unsigned char *append_character;
//unsigned char decode_stack[4000];  /* Holds the decoded string */
//static int input_bit_count=0;    /* Number of bits in input bit buffer */
//static unsigned long input_bit_buffer=0L;
//
//void initLZW()
//{
//   prefix_code = (unsigned int *)malloc(TABLE_SIZE*sizeof(unsigned int));
//   append_character = (unsigned char *)malloc(TABLE_SIZE*sizeof(unsigned char));
//   input_bit_count=0;    /* Number of bits in input bit buffer */
//   input_bit_buffer=0L;
//}
//
//void closeLZW()
//{
//   free(prefix_code);
//   free(append_character);
//}
//
///***************************************************************************
//** setBITS
//**
//** Purpose: To adjust the number of bits used to store codes to the value
//** passed in.
//***************************************************************************/
//int setBITS(int value)
//{
//   if (value == MAXBITS) return TRUE;
//
//   BITS = value;
//   MAX_VALUE = (1 << BITS) - 1;
//   MAX_CODE = MAX_VALUE - 1;
//   return FALSE;
//}
//
///***************************************************************************
//** decode_string
//**
//** Purpose: To return the string that the code taken from the input buffer
//** represents. The string is returned as a stack, i.e. the characters are
//** in reverse order.
//***************************************************************************/
//char *decode_string(unsigned char *buffer,unsigned int code)
//{
//   int i;
//
//   i=0;
//   while (code > 255) {
//      *buffer++ = append_character[code];
//      code=prefix_code[code];
//      if (i++>=4000) {
//         printf("Fatal error during code expansion.\n");
//	      exit(0);
//      }
//   }
//   *buffer=code;
//   return(buffer);
//}
///***************************************************************************
//** input_code
//**
//** Purpose: To return the next code from the input buffer.
//***************************************************************************/
//unsigned int input_code(unsigned char **input)
//{
//   unsigned int return_value;
//
//   while (input_bit_count <= 24) {
//      input_bit_buffer |= (unsigned long) *(*input)++ << input_bit_count;
//      input_bit_count += 8;
//   }
//
//   return_value = (input_bit_buffer & 0x7FFF) % (1 << BITS);
//   input_bit_buffer >>= BITS;
//   input_bit_count -= BITS;
//   return(return_value);
//}
//
///***************************************************************************
//** expand
//**
//** Purpose: To uncompress the data contained in the input buffer and store
//** the result in the output buffer. The fileLength parameter says how
//** many bytes to uncompress. The compression itself is a form of LZW that
//** adjusts the number of bits that it represents its codes in as it fills
//** up the available codes. Two codes have special meaning:
//**
//**  code 256 = start over
//**  code 257 = end of data
//***************************************************************************/
//void expand(unsigned char *input, unsigned char *output, int fileLength)
//{
//   unsigned int next_code, new_code, old_code;
//   int character, index, BITSFull, i;
//   unsigned char *string, *endAddr;
//
//   initLZW();
//
//   BITSFull = setBITS(START_BITS);  /* Starts at 9-bits */
//   next_code = 257;                 /* Next available code to define */
//
//   endAddr = (unsigned char *)((long)output + (long)fileLength);
//
//   old_code = input_code(&input);    /* Read in the first code */
//   character = old_code;
//   new_code = input_code(&input);
//
//   while ((output < endAddr) && (new_code != 0x101)) {
//
//      if (new_code == 0x100) {      /* Code to "start over" */
//	      next_code = 258;
//	      BITSFull = setBITS(START_BITS);
//	      old_code = input_code(&input);
//	      character = old_code;
//         *output++ = (char)character;
//	      new_code = input_code(&input);
//      }
//      else {
//	      if (new_code >= next_code) { /* Handles special LZW scenario */
//	         *decode_stack = character;
//	         string = decode_string(decode_stack+1, old_code);
//	      }
//	      else
//	         string = decode_string(decode_stack, new_code);
//
//         /* Reverse order of decoded string and store in output buffer. */
//	      character = *string;
//	      while (string >= decode_stack)
//            *output++ = *string--;
//
//	      if (next_code > MAX_CODE)
//	         BITSFull = setBITS(BITS + 1);
//
//         prefix_code[next_code] = old_code;
//	      append_character[next_code] = character;
//	      next_code++;
//	      old_code = new_code;
//
//	      new_code = input_code(&input);
//      }
//   }
//
//   closeLZW();
//}
//
///****************************************************************************
//** loadGameSig
//**
//** Purpose: To determine the game ID signature by finding the DIR and VOL.0
//** file and copying what the prefix for each is. If they agree, then it is
//** a valid version 3 game. This function can actually be used to detect the
//** presence of an AGIv3 game rather than read the signature.
//****************************************************************************/
//void loadGameSig(char *gameSig)
//{
//   /*struct ffblk ffblk;
//   char dirString[10]="", volString[10]="", *end;
//   int done;
//
//   done = findfirst("*.*", &ffblk, FA_ARCH);
//   while (!done) {
//      if (end = strstr(ffblk.ff_name, "DIR"))
//	      strncpy(dirString, ffblk.ff_name, end - ffblk.ff_name);
//      if (end = strstr(ffblk.ff_name, "VOL.0"))
//	      strncpy(volString, ffblk.ff_name, end - ffblk.ff_name);
//      done = findnext(&ffblk);
//   }
//
//   if ((strcmp(volString, dirString) == 0) && (volString != NULL))
//      strcpy(gameSig, volString);
//   else
//      strcpy(gameSig, "");*/
//}
