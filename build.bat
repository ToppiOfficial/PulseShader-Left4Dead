@echo off
setlocal
cd /d "%~dp0"

for /f "tokens=*" %%I in ('where python.exe 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
if not defined PYTHON_EXE for /f "tokens=*" %%I in ('where python3.exe 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
if not defined PYTHON_EXE (
  echo ERROR: Python was not found on PATH.
  exit /b 1
)

for /f "tokens=*" %%I in ('where perl.exe 2^>nul') do if not defined PERL_EXE set "PERL_EXE=%%I"
if not defined PERL_EXE if exist "%ProgramFiles%\Git\usr\bin\perl.exe" set "PERL_EXE=%ProgramFiles%\Git\usr\bin\perl.exe"
if not defined PERL_EXE if exist "%ProgramFiles(x86)%\Git\usr\bin\perl.exe" set "PERL_EXE=%ProgramFiles(x86)%\Git\usr\bin\perl.exe"
if not defined PERL_EXE (
  echo ERROR: Perl was not found. Install Git for Windows or put perl.exe on PATH.
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo ERROR: Visual Studio Installer vswhere.exe was not found.
  exit /b 1
)

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT (
  echo ERROR: Visual Studio C++ x86/x64 tools were not found.
  exit /b 1
)

set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
if not exist "%VCVARS%" (
  echo ERROR: %VCVARS% was not found.
  exit /b 1
)

set "CMAKE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"

call "%VCVARS%" >nul || exit /b 1

"%CMAKE%" -E remove_directory "%~dp0dist" || exit /b 1
"%CMAKE%" -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE="%PYTHON_EXE%" -DPERL_EXECUTABLE="%PERL_EXE%" || exit /b 1
"%CMAKE%" --build build || exit /b 1

echo BUILD OK
