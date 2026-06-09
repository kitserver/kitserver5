@echo off
echo Setting kitserver compile environment

if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo ERROR: vswhere.exe not found. Visual Studio 2017+ required.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`
  "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" ^
    -latest ^
    -products * ^
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
    -property installationPath
`) do (
    call "%%i\VC\Auxiliary\Build\vcvars32.bat"
)

if errorlevel 1 (
    echo ERROR: Could not initialize Visual Studio build environment
    pause
    exit /b 1
)

echo Environment set
pause
