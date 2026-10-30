@echo off
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build-windows.ps1 -Architecture x64
exit /b %errorlevel%
