@echo off
setlocal
cd /d "%~dp0"
echo.
echo 125A Dystorator - i9 Windows CPU comparison
echo Before / After / After / Before, 48 configurations per run.
echo.
echo NOTE: This tool does NOT install a VST3, modify your DAW or prove ASIO performance.
echo Results are saved next to this file in the Results folder.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-cpu.ps1" -HostExe "%~dp0Tools\compact_vst3_host.exe" -BeforeVst3 "%~dp0Before\Djenty Donkey.vst3" -AfterVst3 "%~dp0After\Djenty Donkey.vst3" -OutputDir "%~dp0Results"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo Data capture completed. Send the Results folder for evaluation.
) else (
  echo DIAGNOSTIC ERROR %RC% - preserve all files in Results.
)
echo.
pause
exit /b %RC%
