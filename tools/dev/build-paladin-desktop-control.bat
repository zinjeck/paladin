@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc /std:c++20 /O2 /Fe:tools\dev\PaladinDesktopControl.exe tools\dev\PaladinDesktopControl.cpp user32.lib gdi32.lib gdiplus.lib
exit /b %errorlevel%
