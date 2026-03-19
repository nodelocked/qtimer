# 将 qtimer 加入当前用户开机自启动（无需管理员权限）
$exePath = Join-Path $PSScriptRoot "qtimer.exe"

if (-not (Test-Path $exePath)) {
    Write-Host "未找到 qtimer.exe，请确保脚本与 qtimer.exe 在同一目录。" -ForegroundColor Red
    pause
    exit 1
}

$regKey  = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$regName = "qtimer"

Set-ItemProperty -Path $regKey -Name $regName -Value "`"$exePath`""
Write-Host "已加入开机自启动：$exePath" -ForegroundColor Green
pause
