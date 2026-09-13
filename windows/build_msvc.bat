@echo off
setlocal
rem Build from an "x64 Native Tools Command Prompt for VS 2022".
rem No CRT is linked; LayoutBeep uses only Win32 system libraries.

cl /nologo /utf-8 /TC /c /O1 /Oi- /GS- /Zl /W3 /WX LayoutBeep.c
if errorlevel 1 exit /b 1

link /nologo /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS,6.01 /MACHINE:X64 /NODEFAULTLIB /OPT:REF /OPT:ICF ^
  LayoutBeep.obj ^
  kernel32.lib user32.lib gdi32.lib shell32.lib advapi32.lib comdlg32.lib winmm.lib ^
  ole32.lib oleaut32.lib oleacc.lib setupapi.lib ^
  /OUT:LayoutBeep.exe
if errorlevel 1 exit /b 1

del /q LayoutBeep.obj 2>nul
echo Built LayoutBeep.exe
