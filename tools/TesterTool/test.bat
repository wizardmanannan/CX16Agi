cd %4
make
OSFMount -a -t file -f %1 -o rw -m %2:
copy agi.cx16* %2:
copy agi.cx16* %3
rename agi.cx16* AGI.CX16*
timeout 3
OSFMount -D -m %2:
cd C:\Commander\ 
IF  %5=="" ( x16emu.exe -sdcard %~n1%~x1 -prg "agi.cx16" -run -debug d -echo >c:\temp\output.txt -dump V ) else  ( x16emu.exe -sdcard %~n1%~x1 -prg "agi.cx16" -run -debug d -echo %5 >c:\temp\output.txt -dump V )

pause
cd %4