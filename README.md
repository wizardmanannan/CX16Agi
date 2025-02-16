# Commander X16 AGI (Under Construction)

## An AGI Interpreter For The CommanderX 16 Based On Meka By Lance Ewing

**Note: This Project Is Under Construction And Is Not Ready For General Use**

See it running here on YouTube: \
[King's Quest I](https://youtu.be/N0JRAPeKvf0) \
[Space Quest II](https://youtu.be/Pxg_op2dU8Q) \
[King's Quest III](https://youtu.be/BYunlA-CIlE)

![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/95672f9e-32c5-4775-9ee3-5145495f0f11)
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/3fdfeadf-2016-48f1-a2fb-0f577838c710)  
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/3df45e82-c7d4-4b29-ae96-cdc5e560ff09)  
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/13d73012-a379-47b4-b18f-87e80225c55e)  

![image](https://github.com/user-attachments/assets/c1cdc287-a795-44fb-946b-8e7155d80fa1)
![image](https://github.com/user-attachments/assets/96817fbb-d173-4ca7-98f6-d2cb1458ef55)

# CX 16 Iteration 11 Goals (Current)
- Build a reliable, fast and efficient sprite memory allocator
- Fix object placement bugs, but trying to make code function exactly the same and a known good interpreter

# CX 16 Iteration 10 Goals (Complete)
- Implement sprite garbage collection so that the backbuffer (see below) will fit.
- Implement backbuffer for '3D Affect' sprites, which will eliminate IRQ overruns for very large sprites

# CX 16 Iteration 9 Goals (Completed)
- Sprite to background priority
- Using priority screen in interpreter

# CX 16 Iteration 8 Goals (Completed)
- Incorporate CosmicR's fast drawing algorithm for both main and priority screen

# CX 16 Iteration 7 Goals (Completed)
- Allow the user to walk around the map
- Implement a parser and keyboard controls
- Objects

# CX 16 Iteration 6 Goals (Completed)
- Support more games than just King's Quest 3. 

# CX 16 Iteration 5 Goals (Completed)
- Sprite support
- Implement System Tick Replacing Inaccurate Inherited C One

**Only AGI 1 & 2 games are currently supported**

Sierra released many well-known adventure games, like King's Quest and Police Quest. For obvious reasons, these games were not coded from scratch but rather depended upon engines to run the games.

One such engine was the AGI engine, which was used to power games such as King's Quest 1 - 3 and Space Quest 1 and 2.

The goal of this project is to build an AGI Interpreter for the CommanderX16.

https://www.commanderx16.com/forum/index.php?/home/

This is based upon the Meka AGI Interpreter by Lance Ewing and utilizes the code freely available from his website.

Permission to use his code has been obtained in written form: https://github.com/lanceewing/agile/issues/65

The original MEKA source code is included in the root of this repo in an archive called Meka.zip

MEKA is written in C, hence this project depends on CC65. KickC may be considered in a future enhancement.

However extensive assembly is used where speed is required.

## Instructions For Building In Windows:
See: https://github.com/wizardmanannan/CX16Agi/blob/main/How%20To%20Setup%20Meka%20For%20Cx16.docx


Obvious things that need to be done:
- I have been building this under Windows, and the deployment tool I threw together for the purpose is Windows dependent. A Linux build tool should be developed
- String functions need to be uncommented out and made to work
- Import the rest of the MEKA code base. I have been importing it in a piecemeal fashion, and using a stub
- Sound need to be implemented
- Uncomment out routines for the support of AGI 3 games and put them into the BANKS
- Review the implementation of dynamic memory as mentioned in memorymanager.h and the related C file. I am not a memory algorithm expert; there may be a much more efficient way of doing things. At least a review of the segment sizes I have chosen will be required.
- Menu systems need to be built, one for game select (maybe BASIC) and another for the internal game menus
- <s>Priority screens and sprite to background priority</s>

This project requires extensive use of Banked RAM, for both code and data as the Meka source code is quite large.

Due to the sheer number of objects and amount of code to be stored, an allocation spreadsheet tracks this called docs\allocation.xlsx

Code which is stored in banks has a name starting with bX, where X is the bank in HEX where the code is stored, for example, bAFoo.

It is also not possible to keep every AGI object, such as views, in low memory either.

There are two broad kinds of AGI objects. Those of which the size and number are known at compile time and those which aren't.

For those which are, helper functions such as...
```c
void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
...exist for fetching and setting AGI objects to and from memory. These helper functions fetch the objects from the banked RAM and copy them into low memory or vice versa.
```

## Dynamic Memory Management

A system of dynamic banked memory allocation exists for managing objects which are dynamically loaded at runtime. This uses an implementation of the best-fit algorithm, with six sizes of memory allocation spots stored from banks 0x13 - 0x26. This is explained in detail in memorymanager.h and the related C file.

When dynamic memory is needed, `byte* banked_alloc(int size, byte* bank)` from `memorymanager.h` is called to allocate the memory. It works similarly to the standard `malloc` function, returning the address of the memory block and setting the bank at the address in the second argument.

The `boolean banked_dealloc(byte* ptr, byte bank)` function deallocates the memory. The available allocation sizes are described in `allocation.xlsx` in the 'dynamic' tab.

It's worth noting that the algorithm, implementation, and segmentation of memory might not be optimal. As more games are run, a better understanding of the appropriate segment sizes will be gained. Improvements to the segmentation sizes and overall memory management might be needed in the future.

All of the sizes and other numerous constants related to dynamic memory are located in `memorymanager.h`.



