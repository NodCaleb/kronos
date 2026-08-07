@echo off
REM Host test build script for Windows with MinGW GCC (WinLibs)
REM Install: winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
REM Requires cmake (pip install cmake or system cmake)
REM Usage: run_tests.bat [clean]

set WINLIBS_GCC=C:\Users\%USERNAME%\AppData\Local\Microsoft\WinGet\Links\gcc.exe
set CMAKE=python -m cmake

if "%1"=="clean" (
    if exist build rmdir /s /q build
)

%CMAKE% -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug --preset default
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
%CMAKE% --build build
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
build\host_tests.exe

