@echo off
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v qtimer /f >nul 2>&1
if %errorlevel%==0 (echo 已移除开机自启动。) else (echo qtimer 未在自启动列表中。)
pause
