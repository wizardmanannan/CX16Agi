# CX16Agi

**Note This Project Is Under Construction And Is Not Ready For General Use**

**Currently this program is very slow and requires Warp Mode. Significant optimisation will be required**

Sierra released many well known adventure games, like King's Quest and Police Quest. For obvious reasons these games were not coded from scratch, but rather depended upon engines to run the games.

Once such engine was the AGI engine, which was used to power games such as King's Quest 1 - 3 and Space Quest 1 and 2. 

The goal of this project is to build an AGI Interpreter for the CommanderX16. 

https://www.commanderx16.com/forum/index.php?/home/

This is based upon the Meka AGI Interpreter by Lance Ewing, and utilises the code freely available from him website. 

Permission to use his code has been obtained in written form: https://github.com/lanceewing/agile/issues/65

The original MEKA source code is included in the root of this repo in a an archive called Meka.zip

MEKA is written in C, hence this project depends on CC65. KickC may be considered in a future enhancement.

**Instructions For Building In Windows:**
1. Download WinMake and install WinMake from: https://gnuwin32.sourceforge.net/downlinks/make.php
2. Clone https://github.com/wizardmanannan/CX16Agi.git
3. Add WinMake binary directory to your system environment variables. For me the binary directory is: C:\Program Files (x86)\GnuWin32\bin
4. 



Thus far the project:\
#Reads the AGI index files\
#Loads the LOGIC and executes most code (code relying on Views with loops is not yet working. This causes hanging)\
#Prints what is executed to the screen

Obvious things that need to be done:\
#Test more games, I have only tested one game thus far King's Quest III\
#I have been building this under Windows, and the deployment tool I through together for the purpose is Windows dependent. A Linux build tool should be developed\
#String functions need to be uncommented out and retested
#This code is currently very SLOW, this most likely culprit is that the engine unloads and loads resources a lot. I suspect that with a cache of some kind the speed should be a vastly improved\
#Import the rest of the MEKA code base. I have been importing it in a piecemeal fashion, and using a stub\
#More stuff needs to be moved out of the low RAM area, as we are upto a 19kb executable. This is both code and data. Although the vast majority of the code is on the banks already, there are some large file loading routines which I would like to see moved\
#Graphics, sound and keyboard routines need to be implemented\
#Uncomment out routines for the support of AGI 3 games, and put them into the BANKS
#Review the implementation of dynamic memory as mentioned in memorymanager.h and the related C file. I am not a memory algorithm expert, there may be a much more efficent way of doing things. At least a review of the segment sizes I have chosen will be required. 


This project requires extensive use of Banked RAM, for both code and data as the Meka source code is quiet large.

Due to the sheer number of objects and amount of code to be stored an allocation spreadsheet tracks this called docs\allocation.xlsx

Code which is stored in banks has name starting with bX, where X is the bank in HEX where the code is stored, for example bAFoo.

Extensive calling of code between banks in a necessary evil, due to the sheer volume of code. To faciliate this trampoline code exists in helpers.h. 

There are trampolines for the most common function signatures for example: boolean trampoline_1pRetbool(fnTrampoline_1BytePointerPointerRetBool func, byte** data, byte bank) is for functions taking 1 byte pointer as an argument and returning a boolean. 

For functions with unique signatures, individual trampolines may exist.

It is also not possible to keep every AGI object such as views in low memory either. 

There are two broad kinds of AGI objects. Those of which the size and number are known at compile time and those which aren't.

For those which are helper functions such as .  .  .

void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
 
. . . exist within the code file which defines them. These allow one object at a time to be copied to/from stack of the function calling it, regardless of which bank that code is on. 

These objects are stored on specific banks as defined in the allocation spreadsheet. 

memorymanager.h defines sizes, locations of objects in memory, banks and so on.

A system of dynamic banked memory allocation exists for managing objects which are dynamically loaded at runtime

This uses my implementation of best fit algorithm, there are six sizes of memory allocation spots stored from banks 16 - 39. 

When dynamic memory is needed 'byte* banked_alloc(int size, byte* bank)' from memorymanager.h is called to allocate the memory. In works very much like the malloc we all understand, the address of the memory block is returned, the key difference between that a bank is also set at the address in the second argument. 

boolean banked_dealloc(byte* ptr, byte bank) conversely deallocates the memory.

The available allocation sizes are described in allocation.xlsx in the 'dynamic' tab

I cannot claim that this is the best algorithm, implementation or segmentation of memory. As I mentioned above work will have to be done to come up with better segmentation sizes, I took a guess when I made them.
As more games are run we will have a better idea of what segment sizes are best.

All of the sizes and other numerous constants related to dynamic memory are location in memory manager.h




