@echo off
echo Setting kitserver compile environment (legacy VS)

if exist "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" (
    call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x86
    goto :ok
)

if exist "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" (
    call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x86
    goto :ok
)

if exist "%VS110COMNTOOLS%..\..\VC\vcvarsall.bat" (
    call "%VS110COMNTOOLS%..\..\VC\vcvarsall.bat" x86
    goto :ok
)

if exist "%VS100COMNTOOLS%..\..\VC\vcvarsall.bat" (
    call "%VS100COMNTOOLS%..\..\VC\vcvarsall.bat" x86
    goto :ok
)

if exist "%VS90COMNTOOLS%..\..\VC\vcvarsall.bat" (
    call "%VS90COMNTOOLS%..\..\VC\vcvarsall.bat" x86
    goto :ok
)

echo ERROR: No supported Visual Studio VC toolchain found (expected VS2008-2015 with VC++)
echo Hint: Make sure "Visual C++" / "VC++ build tools" is installed for that VS version.
pause
exit /b 1

:ok
echo Environment set
pause
