@echo off

REM Create build directory if it doesn't exist
if not exist "build\debug" mkdir "build\debug"

REM Compile main.c with debug symbols
cl.exe /Od /Zi /Fo:"build\debug\main.obj" /Fd:"build\debug\client.pdb" /Fe:"build\debug\client.exe" src\main.c /link /DEBUG winhttp.lib
