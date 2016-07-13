goto q
:h
pause
:q
cls
del floating.exe
if exist floating.exe goto h
mingw32-make -j4 -k
floating.exe
goto h