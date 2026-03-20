@echo off
set "EXE=%~dp0qtimer.exe"
if not exist "%EXE%" (
    echo 未找到 qtimer.exe，请确保脚本与 qtimer.exe 在同一目录。
    pause & exit /b 1
)
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v qtimer /t REG_SZ /d "\"%EXE%\"" /f >nul
echo 已加入开机自启动。
pause
