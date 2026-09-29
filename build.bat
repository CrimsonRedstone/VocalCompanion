@echo off
REM Vocal Companion - Windows one-click VST3/CLAP build
REM Crimson Redstone / Freeware
REM Double-click this file. The window stays open until you press a key.

setlocal EnableExtensions
cd /d "%~dp0"
set "ROOT=%CD%"
set "STATUS=%ROOT%\BUILD_STATUS.txt"
set "VC_LOG=%ROOT%\build.log"
set "VC_BAT=%~f0"
set "ERR=1"

if /I "%~1"=="--inner" goto :work
if /I "%~1"=="--stay" goto :tee
if /I "%~1"=="nopause" goto :tee
if /I "%~1"=="--nopause" goto :tee

REM Explorer launches with "cmd /c", which closes the window when the script
REM ends. Re-open with /k so a missing pause can never flash-close the window.
echo %cmdcmdline% | findstr /I /C:" /c " >nul
if errorlevel 1 goto :tee
cmd /k call "%~f0" --stay
exit /b

:tee
set "DOPAUSE=1"
if /I "%~1"=="nopause" set "DOPAUSE=0"
if /I "%~1"=="--nopause" set "DOPAUSE=0"
echo Logging to: %VC_LOG%
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tee-build.ps1"
set "ERR=%ERRORLEVEL%"
echo.
echo Log file: %VC_LOG%
if not "%DOPAUSE%"=="1" goto :leave
echo Press any key to close this window.
pause >nul
goto :leave

:work
echo.
echo ============================================================
echo   Vocal Companion  -  plugin build
echo   Crimson Redstone / Freeware
echo   Folder: %ROOT%
echo   Log:    %VC_LOG%
echo ============================================================
echo.

if exist "%ROOT%\CMakeLists.txt" goto :have_lists
set "FAILMSG=CMakeLists.txt is not next to build.bat. Extract the ZIP to a folder first, then double-click build.bat inside VocalCompanion."
goto :fail

:have_lists
REM Two-step x86 Program Files path. Do not concatenate that env var
REM with a backslash-Microsoft on the same line.
set "PF86=%ProgramFiles(x86)%"
if not defined PF86 set "PF86=C:\Program Files (x86)"

REM ---- CMake ----------------------------------------------------------------
set "CMAKE="
where cmake >nul 2>&1
if errorlevel 1 goto :cmake_fallback
for /f "delims=" %%P in ('where cmake') do (
    set "CMAKE=%%P"
    goto :have_cmake
)

:cmake_fallback
if exist "C:\Program Files\CMake\bin\cmake.exe" set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
if not defined CMAKE if exist "%PF86%\CMake\bin\cmake.exe" set "CMAKE=%PF86%\CMake\bin\cmake.exe"
if not defined CMAKE if exist "%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe" set "CMAKE=%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe"

:have_cmake
if defined CMAKE goto :cmake_ok
echo ERROR: CMake is not installed, or it is not on PATH.
echo.
echo   1. Download  https://cmake.org/download/
echo   2. Run the installer
echo   3. Tick "Add CMake to the system PATH for all users"
echo   4. Close this window and double-click build.bat again
echo.
set "FAILMSG=CMake not found."
goto :fail

:cmake_ok
for %%P in ("%CMAKE%") do set "VC_CMAKEBIN=%%~dpP"
set "PATH=%VC_CMAKEBIN%;%PATH%"
echo [1/5] Tools
echo       CMake: %CMAKE%
"%CMAKE%" --version
if errorlevel 1 (
    set "FAILMSG=CMake exists but failed to run."
    goto :fail
)

REM ---- Git ------------------------------------------------------------------
set "GIT="
where git >nul 2>&1
if errorlevel 1 goto :git_fallback
for /f "delims=" %%P in ('where git') do (
    set "GIT=%%P"
    goto :have_git
)

:git_fallback
if exist "C:\Program Files\Git\cmd\git.exe" set "GIT=C:\Program Files\Git\cmd\git.exe"
if not defined GIT if exist "%PF86%\Git\cmd\git.exe" set "GIT=%PF86%\Git\cmd\git.exe"
if not defined GIT if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "GIT=%LOCALAPPDATA%\Programs\Git\cmd\git.exe"

:have_git
if defined GIT goto :git_ok
echo.
echo ERROR: Git is not installed. CMake needs Git to download JUCE.
echo   Download  https://git-scm.com/download/win
echo   Install with the defaults, then re-run build.bat
echo.
set "FAILMSG=Git not found."
goto :fail

:git_ok
REM NEVER set GIT_DIR. Git treats that as "the .git folder of a repo" and
REM FetchContent then dies with: fatal: not a git repository: ...\Git\cmd\
for %%P in ("%GIT%") do set "VC_GITBIN=%%~dpP"
set "PATH=%VC_GITBIN%;%PATH%"
set GIT_DIR=
set GIT_WORK_TREE=
set GIT_COMMON_DIR=
set GIT_OBJECT_DIRECTORY=
echo       Git:   %GIT%

REM ---- Visual Studio --------------------------------------------------------
set "VSROOT="
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :vs_missing

"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vc-vswhere.txt" 2>nul
set "VSROOT="
set /p VSROOT=<"%TEMP%\vc-vswhere.txt"
if not defined VSROOT goto :vs_missing
echo       VS:    %VSROOT%
goto :vs_done

:vs_missing
echo       VS:    not found via vswhere
echo              Install Visual Studio 2022 with "Desktop development with C++"
echo              https://visualstudio.microsoft.com/downloads/

:vs_done
echo.

REM Drop a half-cloned JUCE tree from a previous failed run.
if exist "%ROOT%\build\_deps\juce-src\.git" goto :cfg
if not exist "%ROOT%\build\_deps" goto :cfg
echo Removing incomplete JUCE download from a previous run...
rd /s /q "%ROOT%\build\_deps"

:cfg
echo [2/5] Configuring CMake
echo       First run downloads JUCE 9.0.1. This can take several minutes.
echo       Live output follows. Do not close this window.
echo       Log: %VC_LOG%
echo.

"%CMAKE%" -S "%ROOT%" -B "%ROOT%\build" -G "Visual Studio 17 2022" -A x64 -DGIT_EXECUTABLE="%GIT%"
if not errorlevel 1 goto :configured

echo.
echo CMake configure failed.
echo If you see "fatal: not a git repository" this was a GIT_DIR bug - use this new build.bat.
echo If GitHub is unreachable, check the network, then delete the build folder and retry.
echo Full log: %VC_LOG%
set "FAILMSG=CMake configure failed. See build.log"
goto :fail

:configured
echo.
echo [3/5] Compiling Release. This takes several minutes...
echo.
"%CMAKE%" --build "%ROOT%\build" --config Release --parallel
if not errorlevel 1 goto :compiled
echo.
echo Compile failed. Scroll up for the first error.
echo Full log: %VC_LOG%
set "FAILMSG=Compile failed. See build.log"
goto :fail

:compiled
echo.
echo [4/5] Locating plugin files...
set "ART="
for /d %%D in ("%ROOT%\build\VocalCompanion_artefacts*") do set "ART=%%~fD"
if not defined ART for /d %%D in ("%ROOT%\build\*\VocalCompanion_artefacts*") do set "ART=%%~fD"

echo.
echo [5/5] BUILD OK
echo ============================================================
if not defined ART goto :no_art
echo   Output: %ART%
echo.
if exist "%ART%\Release" goto :list_rel
dir /s /b "%ART%\*.vst3" 2>nul
dir /s /b "%ART%\*.clap" 2>nul
goto :copy_hint

:list_rel
dir /s /b "%ART%\Release\*.vst3" 2>nul
dir /s /b "%ART%\Release\*.clap" 2>nul
goto :install_vst3

:no_art
echo   Search %ROOT%\build for .vst3 and .clap
goto :copy_hint

:install_vst3
set "VST3SRC="
for /f "delims=" %%F in ('dir /s /b /ad "%ART%\*.vst3" 2^>nul') do set "VST3SRC=%%F"
if not defined VST3SRC goto :copy_hint
set "VST3DST=%COMMONPROGRAMFILES%\VST3\VocalCompanion.vst3"
echo.
echo Installing VST3 to:
echo   %VST3DST%
xcopy /E /I /H /Y "%VST3SRC%" "%VST3DST%" >nul 2>&1
if not errorlevel 1 (
    attrib +s "%VST3DST%"
    attrib +s +h "%VST3DST%\desktop.ini"
    echo   Copied to Common Files\VST3
    goto :copy_hint
)
set "VST3DST=%LOCALAPPDATA%\Programs\Common\VST3\VocalCompanion.vst3"
echo   Common Files was not writable. Trying:
echo   %VST3DST%
xcopy /E /I /H /Y "%VST3SRC%" "%VST3DST%" >nul 2>&1
if not errorlevel 1 (
    attrib +s "%VST3DST%"
    attrib +s +h "%VST3DST%\desktop.ini"
    echo   Copied. Add that folder in FL Studio if it does not scan it.
) else (
    echo   Auto-install failed. Copy the .vst3 folder manually.
)

:copy_hint

:no_art
echo   Search %ROOT%\build for .vst3 and .clap

:copy_hint
echo.
echo   Copy the .vst3 folder to: C:\Program Files\Common Files\VST3
echo   Copy the .clap file  to: C:\Program Files\Common Files\CLAP
echo   Log: %VC_LOG%
echo ============================================================
(
    echo SUCCESS
    echo time=%DATE% %TIME%
    echo artefacts=%ART%
    echo log=%VC_LOG%
) > "%STATUS%"
set "ERR=0"
goto :done

:fail
set "ERR=1"
echo.
echo ============================================================
echo   BUILD FAILED
if defined FAILMSG echo   %FAILMSG%
echo   Folder: %ROOT%
echo   Log:    %VC_LOG%
echo ============================================================
(
    echo FAILED
    echo time=%DATE% %TIME%
    echo reason=%FAILMSG%
    echo log=%VC_LOG%
) > "%STATUS%"

:done
exit /b %ERR%

:leave
endlocal & exit /b %ERR%
