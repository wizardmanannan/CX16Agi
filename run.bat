make
OSFMount -a -t file -f "C:\Commander\sdcard.img" -o rw -m E:
copy agi.cx16* "E:"
copy agi.cx16* "C:\Commander"
rename agi.cx16* AGI.CX16*
timeout 3
cd C:\Commander\
OSFMount -D -m E:
x16emu.exe -sdcard sdcard.img -prg "agi.cx16" -run -via2 -warp -echo -debug d -dump V 1>c:\temp\output.txt 
cd c:\meka\CommanderX16Repo