@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0run-unity-godot-parity.ps1" %*
exit /b %ERRORLEVEL%
