@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0capture-unity-parity.ps1" %*
exit /b %ERRORLEVEL%
