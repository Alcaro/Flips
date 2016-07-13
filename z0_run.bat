goto q
:h
pause
:q
cls
del floating.exe
if exist floating.exe goto h
mingw32-make -j4
floating.exe
goto h