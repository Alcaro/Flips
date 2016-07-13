rem del /q obj\*
del floating.exe
mingw32-make OBJSUFFIX="-opt" OPT=1 -j4 OUTNAME=floating.exe

rem del floating64.exe
rem mingw32-make CC=gcc64 CXX=g++64 LD=g++64 RCFLAGS="-Fpe-x86-64" OBJSUFFIX="-opt64" OPT=1 -j4 OUTNAME=floating64.exe
pause
