::This script creates a heavily optimized binary. For debugging, you're better off using 'mingw32-make'.

call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
::call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
::call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars64.bat"

goto q
:h
pause
:q
cls
del flips.exe
if exist flips.exe goto h
::/c - don't link
::/Oy - omit frame pointer
::/Oi - use intrinsics
::/Os - smaller code
::/Ox - all optimizations
::/EHs-c- - no exceptions
::/Gz - use stdcall
::/D_CRT_bluh - shut the fuck up about strcpy
::/WX - enable warnings
::/wd4700 - uninitialized variable, I know what I'm doing
::/nologo - I know your version, shut up
::/TP - force C++ because void foo(int bar = 0); is syntax error in C
::/MT - use static msvcrt
::/GL - LTCG
::/openmp - enable OpenMP
::cl /c /Oy /Oi /Os /Ox /EHs-c- /Gz /D_CRT_SECURE_NO_WARNINGS /WX /wd4700 /nologo /TP /MT /GL *.cpp *.c
rem /openmp 
cl /c /Oy /Oi /Os /Ox /EHs-c- /Gz /D_CRT_SECURE_NO_WARNINGS /WX /wd4700 /nologo /TP /MT /GL /Imsvc ^
   /DUSE_DIVSUFSORT /Ilibdivsufsort-2.0.1\include /DHAVE_CONFIG_H libdivsufsort-2.0.1\lib\*.c ^
   *.c *.cpp
rc flips.rc

link /LTCG /nologo *.obj /subsystem:windows flips.res user32.lib gdi32.lib comctl32.lib shell32.lib comdlg32.lib advapi32.lib /out:flips.exe

::FOR %%G IN (2dland-dec2013,smwcp,nsmw-tll,senate13,kamek) DO (
::FOR %%G IN (2dland-dec2013) DO (
::flips --create --ips profile/smw.smc profile/%%G.smc profile/tmp
::flips --apply  --ips profile/tmp profile/smw.smc NUL.smc

::flips --create --bps-linear profile/smw.smc profile/%%G.smc profile/tmp
::flips --apply  --bps-linear profile/tmp profile/smw.smc NUL.smc

::flips --create --bps-delta profile/smw.smc profile/%%G.smc profile/tmp
::flips --apply  --bps-delta profile/tmp profile/smw.smc NUL.smc
::)
::del profile\tmp

::link /nologo /ltcg:pgoptimize *.obj /subsystem:windows flips.res user32.lib gdi32.lib comctl32.lib shell32.lib comdlg32.lib advapi32.lib /out:flips.exe

del *.obj flips.res *.pgc *.pgd
goto h

::kernel32.lib shell32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib dinput8.lib
