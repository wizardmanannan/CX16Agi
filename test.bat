make
OSFMount -a -t file -f "C:\Commander\sdcard.img" -o rw -m E:
copy agi.cx16* "E:"
copy agi.cx16* "C:\Commander"
rename agi.cx16* AGI.CX16*
timeout 3
OSFMount -D -m E:
cd C:\Commander\ 
x16emu.exe -sdcard "sdcard.img" -prg "agi.cx16" -run -debug d -warp
cd C:\meka\CommanderX16Version\

