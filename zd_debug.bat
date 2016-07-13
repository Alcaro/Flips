goto q
:h
pause
:q
cls
del floating.exe
if exist floating.exe goto h
mingw32-make -j4
if not exist floating.exe goto h
gdb floating.exe
goto h