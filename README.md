# CX16Agi

**Note This Project Is Under Construction And Is Not Ready For General Use**

Sierra released many well known adventure games, like King's Quest and Police Quest. For obvious reasons these games were not coded from scratch, but rather depended upon engines to run the games.

Once such engine was the AGI engine, which was used to power games such as King's Quest 1 - 3 and Space Quest 1 and 2. 

The goal of this project is to build an AGI Interpreter for the CommanderX16. 

https://www.commanderx16.com/forum/index.php?/home/

This is based upon the Meka AGI Interpreter by Lance Ewing, and utilises the code freely available from him website. 

Permission to use his code has been obtained in written form: https://github.com/lanceewing/agile/issues/65

Thus far the project:
#Reads the AGI index files
#Loads the LOGIC and executes most code (code relying on Views with loops is not yet working. This causes hanging)
#Prints what is executed to the screen

Obvious things that need to be done:
#Test more games, I have only tested one game thus far King's Quest III
#I have been building this under Windows, and the deployment tool I through together for the purpose is Windows dependent. A Linux build tool should be developed
#Load Views Into Memory Likely Ending The Hanging Issue With Loops
#This code is currently very SLOW, this most likely culprit is that the engine unloads and loads resources a lot. I suspect that with a cache of some kind the speed should be a vastly improved
#Import the rest of the MEKA code base. I have been importing it in a piecemeal fashion, and using a stub
#More stuff needs to be moved out of the low RAM area, as we are upto a 19kb executable. This is both code and data. Although the vast majority of the code is on the banks already, there are some large file loading routines which I would like to seem moved
#Graphics, sound and keyboard routines need to be implemented
#Uncomment out routines for the support of AGI 3 games, and put them into the BANKS






