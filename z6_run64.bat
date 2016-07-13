goto q
:h
pause
:q
cls
del floating64.exe
if exist floating64.exe goto h
mingw32-make CC=gcc64 CXX=g++64 LD=g++64 RCFLAGS="-Fpe-x86-64" OBJSUFFIX="-64" -j4 OUTNAME=floating64.exe
floating64.exe
goto h