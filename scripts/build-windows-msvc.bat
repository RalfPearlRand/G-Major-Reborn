@echo off
rem G-Major Reborn - Windows build with Visual Studio 2022 (x64)
rem Optional: set JUCE_DIR=C:\path\to\JUCE  and/or  set ASIO_SDK=C:\path\to\asiosdk
setlocal
set ARGS=-DGMR_BUILD_TESTS=ON -DGMR_BUILD_TOOLS=ON
if defined JUCE_DIR set ARGS=%ARGS% -DGMR_JUCE_DIR="%JUCE_DIR%"
if defined ASIO_SDK set ARGS=%ARGS% -DGMR_ASIO_SDK_DIR="%ASIO_SDK%"
cmake -S "%~dp0.." -B "%~dp0..\build" -G "Visual Studio 17 2022" -A x64 %ARGS% || exit /b 1
cmake --build "%~dp0..\build" --config Release --parallel || exit /b 1
ctest --test-dir "%~dp0..\build" -C Release --output-on-failure || exit /b 1
echo.
echo VST3:       %~dp0..\build\plugin\GMajorReborn_artefacts\Release\VST3
echo Standalone: %~dp0..\build\plugin\GMajorReborn_artefacts\Release\Standalone
