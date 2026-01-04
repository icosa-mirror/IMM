@echo off
REM Copy all required DLLs to a Unity project
REM Usage: CopyDllsToUnity.bat "C:\Path\To\YourUnityProject"

setlocal

if "%~1"=="" (
    echo Usage: CopyDllsToUnity.bat "C:\Path\To\YourUnityProject"
    echo Example: CopyDllsToUnity.bat "C:\Projects\MyUnityProject"
    exit /b 1
)

set UNITY_PROJECT=%~1
set PLUGIN_DIR=%UNITY_PROJECT%\Assets\Plugins\x86_64
set IMM_ROOT=%~dp0..\..\..\

echo.
echo ============================================
echo Copying IMM Unity Plugin DLLs
echo ============================================
echo.
echo Unity Project: %UNITY_PROJECT%
echo Plugin Directory: %PLUGIN_DIR%
echo IMM Root: %IMM_ROOT%
echo.

REM Create the plugin directory if it doesn't exist
if not exist "%PLUGIN_DIR%" (
    echo Creating directory: %PLUGIN_DIR%
    mkdir "%PLUGIN_DIR%"
)

echo Copying DLLs...
echo.

REM Copy ImmUnityPlugin.dll
echo [1/10] ImmUnityPlugin.dll
copy /Y "%IMM_ROOT%code\appImmUnity\exe\ImmUnityPlugin.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy ImmUnityPlugin.dll
    exit /b 1
)

REM Copy Audio360.dll
echo [2/10] Audio360.dll
copy /Y "%IMM_ROOT%thirdparty\audio360-sdk\Audio360\Windows\x64\Audio360.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy Audio360.dll
    exit /b 1
)

REM Copy jpeg62.dll
echo [3/10] jpeg62.dll
copy /Y "%IMM_ROOT%thirdparty\libjpeg-turbo\bin\jpeg62.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy jpeg62.dll
    exit /b 1
)

REM Copy libpng16.dll
echo [4/10] libpng16.dll
copy /Y "%IMM_ROOT%thirdparty\libpng\bin\libpng16.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy libpng16.dll
    exit /b 1
)

REM Copy ogg.dll
echo [5/10] ogg.dll
copy /Y "%IMM_ROOT%thirdparty\libogg\bin\ogg.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy ogg.dll
    exit /b 1
)

REM Copy opus.dll
echo [6/10] opus.dll
copy /Y "%IMM_ROOT%thirdparty\opus\bin\opus.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy opus.dll
    exit /b 1
)

REM Copy opusenc.dll
echo [7/10] opusenc.dll
copy /Y "%IMM_ROOT%thirdparty\libopusenc\bin\opusenc.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy opusenc.dll
    exit /b 1
)

REM Copy vorbis.dll
echo [8/10] vorbis.dll
copy /Y "%IMM_ROOT%thirdparty\libvorbis\bin\vorbis.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy vorbis.dll
    exit /b 1
)

REM Copy vorbisenc.dll
echo [9/10] vorbisenc.dll
copy /Y "%IMM_ROOT%thirdparty\libvorbis\bin\vorbisenc.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy vorbisenc.dll
    exit /b 1
)

REM Copy zlib1.dll
echo [10/10] zlib1.dll
copy /Y "%IMM_ROOT%thirdparty\zlib\bin\zlib1.dll" "%PLUGIN_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy zlib1.dll
    exit /b 1
)

echo.
echo ============================================
echo SUCCESS! All DLLs copied successfully
echo ============================================
echo.
echo DLLs are now in: %PLUGIN_DIR%
echo.
echo Next steps:
echo 1. Copy the C# scripts from IMM\code\appImmUnity\Scripts to
echo    %UNITY_PROJECT%\Assets\Plugins\ImmPlayer\
echo 2. Open Unity and configure the DLL import settings
echo 3. See README.md for detailed setup instructions
echo.

endlocal
