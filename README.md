# CX16Agi Interation 2

Major Changes Since Iteration 2:
-File Loading Cache Implemented Significantly Improving Speed
- Core of interpreter has been rewritten in 6502 assembly improving speed
- Size of executable has been reduced by shifing more code into banked RAM.

**Note This Project Is Under Construction And Is Not Ready For General Use**

**Currently this program is too slow. Optimisation will be required, but it is many times faster than iteration 1**

**Only AGI 1 & 2 games are currently supported**

Sierra released many well known adventure games, like King's Quest and Police Quest. For obvious reasons these games were not coded from scratch, but rather depended upon engines to run the games.

Once such engine was the AGI engine, which was used to power games such as King's Quest 1 - 3 and Space Quest 1 and 2. 

The goal of this project is to build an AGI Interpreter for the CommanderX16. 

https://www.commanderx16.com/forum/index.php?/home/

This is based upon the Meka AGI Interpreter by Lance Ewing, and utilises the code freely available from him website. 

Permission to use his code has been obtained in written form: https://github.com/lanceewing/agile/issues/65

The original MEKA source code is included in the root of this repo in an archive called Meka.zip

MEKA is written in C, hence this project depends on CC65. KickC may be considered in a future enhancement.

**Instructions For Building In Windows:**
1. Download WinMake and install WinMake from: https://gnuwin32.sourceforge.net/downlinks/make.php
2. Clone https://github.com/wizardmanannan/CX16Agi.git
3. Add WinMake binary directory to your system environment variables. For me the binary directory is: C:\Program Files (x86)\GnuWin32\bin
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
11. Copy the following files from your game folder to the root of an SD card:
-LOGDIR
-SNDDIR
-VIEWDIR
-VOL.0 to VOL.X
-WORDS.TOK
11. Copy the bank config file 'cx16-bank.cfg' to the cc65\cfg. Don't forget to back up the old one.
11. Run 'tester.exe' as admin. Note: The source code for the tester tool is under: \CommanderX16Repo\tools\TesterTool if you would rather compile it yourself
12. Fill in the text boxes according to the instructions in the Window. Note that the make file is located in the root directory of the repo
13. Click 'Test'. Note to run again rerun the tester.exe, it will remember the parameters entered last time. Delete: 'cx16TesterConfig.json' to clear the memory.


Thus far the project:\
#Reads the AGI index files\
#Loads the LOGIC and executes most code
#Prints the script executing to the screen. There is a DEBUG define variable in global.s which can be uncommented out which means that individual instructions and results are printed to screen but that is SLOW

Obvious things that need to be done:\
#Test more games, I have only tested one game thus far King's Quest III\
#I have been building this under Windows, and the deployment tool I threw together for the purpose is Windows dependent. A Linux build tool should be developed\
#String functions need to be uncommented out and retested
#This code is currently slow. It takes 1:10 to execute the first intro screen of King's Quest III, when it should take 45 seconds
#Import the rest of the MEKA code base. I have been importing it in a piecemeal fashion, and using a stub\
#More stuff needs to be moved out of the low RAM area, as we are upto a 19kb executable. This is both code and data. Although the vast majority of the code is on the banks already, there are some large file loading routines which I would like to see moved\
#Graphics, sound and keyboard routines need to be implemented\
#Uncomment out routines for the support of AGI 3 games, and put them into the BANKS
#Review the implementation of dynamic memory as mentioned in memorymanager.h and the related C file. I am not a memory algorithm expert, there may be a much more efficent way of doing things. At least a review of the segment sizes I have chosen will be required. 
#Menu systems needs to be build, one for game select (maybe BASIC) and another from the internal game menus

This project requires extensive use of Banked RAM, for both code and data as the Meka source code is quiet large.

Due to the sheer number of objects and amount of code to be stored an allocation spreadsheet tracks this called docs\allocation.xlsx

Code which is stored in banks has name starting with bX, where X is the bank in HEX where the code is stored, for example bAFoo.

Extensive calling of code between banks in a necessary evil, due to the sheer volume of code. To faciliate this trampoline code exists in helpers.h. 

There are trampolines for the common function signatures for example: boolean trampoline_1pRetbool(fnTrampoline_1BytePointerPointerRetBool func, byte** data, byte bank) is for functions taking 1 byte pointer as an argument and returning a boolean. 

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




