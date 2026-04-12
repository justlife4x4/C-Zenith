@echo off
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo Error: vcvars64.bat not found. Please update the path in build.bat.
    exit /b 1
)

echo Initializing MSVC environment...
call "%VCVARS%" > nul

echo.
echo === Building C-Zenith Static Library ===
echo.

cl /nologo /W3 /c /I./include src/*.c /O2 /DNDEBUG

if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo Creating zenith.lib...
lib /nologo /OUT:zenith.lib *.obj

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: zenith.lib created successfully.
    echo To use: Include 'include/zenith.h' and link with 'zenith.lib'.
) else (
    echo Library creation failed.
    exit /b %ERRORLEVEL%
)

:: Cleanup temp objects
del *.obj > nul
