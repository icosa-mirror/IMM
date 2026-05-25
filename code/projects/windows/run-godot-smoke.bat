@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0run-godot-smoke.ps1" %*
exit /b %ERRORLEVEL%
