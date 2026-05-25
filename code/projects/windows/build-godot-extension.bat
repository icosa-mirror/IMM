@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0build-godot-extension.ps1" %*
exit /b %ERRORLEVEL%
