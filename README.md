
![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/9ed06d79-c4a0-4dc0-8b38-60443c7ac64f)


![image](https://github.com/wizardmanannan/CX16Agi/assets/58645812/a06b9c0d-888e-4b68-929a-458aa8f09392)

  

# CX 16 Iteration 4 Goals (Completed)
- Support AGI Textboxes
- Implement A Loading Screen For the Background Graphics Drawing to Prevent Screen Tearing

# CX 16 Iteration 5 Goals (Current)
- Sprite support
- Implement System Tick Replacing Inaccurate Inherited C One

# CX 16 Iteration 6 Goals (Next)
- Use a scanline screen drawing method to improve loading screen
- Draw priority screen
- Sprites priority with background image will be determined by the priority screen

**Note: This Project Is Under Construction And Is Not Ready For General Use**

Here is a quick demonstration video: https://youtu.be/IDdc890T_oY 

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
1. Download WinMake and install WinMake from: https://gnuwin32.sourceforge.net/downlinks/make.php
2. Clone https://github.com/wizardmanannan/CX16Agi.git
3. Add WinMake binary directory to your system environment variables. For me, the binary directory is: C:\Program Files (x86)\GnuWin32\bin
4. Download the CC65 Compiler from https://sourceforge.net/projects/cc65/files/cc65-snapshot-win32.zip/download
5. Unzip and copy onto your hard drive
6. Add the CC65 bin folder to your system environment variables. Mine is: C:\cc65\bin
7. Navigate to the directory CX16Agi directory in a command prompt, the root of the repo
8. Type 'Make'
9. Download and install 'OSFMount' from: https://www.osforensics.com/tools/mount-disk-images.html
10. Even if you already have it installed add OSFMount to the path folder in the environment variables. Mine is: C:\Program Files\OSFMount
11. Get an AGI 1 or 2 game. Games cannot be provided for obvious reasons. Check out http://agiwiki.sierrahelp.com/index.php?title=Sierra_AGI_Release_List for a list.
Note many games can be legally purchased on GOG https://www.gog.com/.
Also note due to the immaturity of this project all games are not yet guaranteed to work
12. Copy the following files from your game folder to the root of an SD card:
-LOGDIR
-SNDDIR
-VIEWDIR
-VOL.0 to VOL.X
-WORDS.TOK
13. Copy the bank config file 'cx16-bank.cfg' to the cc65\cfg. Don't forget to back up the old one.
14. Run 'tester.exe' as admin. Note: The source code for the tester tool is under: \CommanderX16Repo\tools\TesterTool if you would rather compile it yourself
15. This will run a script which will create an image of your SD card, mount it, and copy the required files onto it. The SD card image will be located in the 'CX16Agi' folder.

Thus far the project:
- Reads the AGI index files
- Loads the LOGIC and executes most code
- Prints the script executing to the screen. There is a DEBUG define variable in global.s which can be uncommented out which means that individual instructions and results are printed to screen but that is SLOW

Obvious things that need to be done:
- Test more games, I have only tested one game thus far King's Quest III
- I have been building this under Windows, and the deployment tool I threw together for the purpose is Windows dependent. A Linux build tool should be developed
- String functions need to be uncommented out and retested
- This code is currently slow. It takes 1:10 to execute the first intro screen of King's Quest III, when it should take 45 seconds
- Import the rest of the MEKA code base. I have been importing it in a piecemeal fashion, and using a stub
- Sprites and textbox, sound, and keyboard routines need to be implemented
- Uncomment out routines for the support of AGI 3 games and put them into the BANKS
- Review the implementation of dynamic memory as mentioned in memorymanager.h and the related C file. I am not a memory algorithm expert; there may be a much more efficient way of doing things. At least a review of the segment sizes I have chosen will be required.
- Menu systems need to be built, one for game select (maybe BASIC) and another for the internal game menus
- Implement a proper system tick function using IRQs. Currently I have using the C one inherited from Meka, which I don't think is very accurate on CX16.

This project requires extensive use of Banked RAM, for both code and data as the Meka source code is quite large.

Due to the sheer number of objects and amount of code to be stored, an allocation spreadsheet tracks this called docs\allocation.xlsx

Code which is stored in banks has a name starting with bX, where X is the bank in HEX where the code is stored, for example, bAFoo.

Extensive calling of code between banks is a necessary evil, due to the sheer volume of code. To facilitate this, trampoline code exists in helpers.h.

There are trampolines for the common function signatures, for example: boolean trampoline_1pRetbool(fnTrampoline_1BytePointerPointerRetBool func, byte** data, byte bank) is for functions taking 1 byte pointer as an argument and returning a boolean.

For functions with unique signatures, individual trampolines may exist.

It is also not possible to keep every AGI object, such as views, in low memory either.

There are two broad kinds of AGI objects. Those of which the size and number are known at compile time and those which aren't.

For those which are, helper functions such as...
```c
void getLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
void setLogicEntry(LOGICEntry* logicEntry, byte logicFileNo);
...exist for fetching and setting AGI objects to and from memory. These helper functions fetch the objects from the banked RAM and copy them into low memory or vice versa.
```

## Dynamic Memory Management

A system of dynamic banked memory allocation exists for managing objects which are dynamically loaded at runtime. This uses an implementation of the best-fit algorithm, with six sizes of memory allocation spots stored from banks 0x13 - 0x3a. This is explained in detail in memorymanager.h and the related C file.


When dynamic memory is needed, `byte* banked_alloc(int size, byte* bank)` from `memorymanager.h` is called to allocate the memory. It works similarly to the standard `malloc` function, returning the address of the memory block and setting the bank at the address in the second argument.

The `boolean banked_dealloc(byte* ptr, byte bank)` function deallocates the memory. The available allocation sizes are described in `allocation.xlsx` in the 'dynamic' tab.

It's worth noting that the algorithm, implementation, and segmentation of memory might not be optimal. As more games are run, a better understanding of the appropriate segment sizes will be gained. Improvements to the segmentation sizes and overall memory management might be needed in the future.

All of the sizes and other numerous constants related to dynamic memory are located in `memorymanager.h`.



