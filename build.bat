@echo off
REM Configure + build 125A Dystorator (Release). First run downloads JUCE + ASIO SDK.
REM Build dir: short path (Windows 260-char limit) and NOT under %TEMP% (MSBuild incremental builds break there).
set BUILD_DIR=%USERPROFILE%\vab-build
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1
echo.
echo Done:
echo   Standalone: %BUILD_DIR%\Dystorator_artefacts\Release\Standalone\125A Dystorator.exe
echo   VST3:       %BUILD_DIR%\Dystorator_artefacts\Release\VST3\125A Dystorator.vst3
echo   DSP tests:  %BUILD_DIR%\Release\dsp_test.exe
