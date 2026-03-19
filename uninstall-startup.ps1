# 将 qtimer 从开机自启动移除
$regKey  = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$regName = "qtimer"

if (Get-ItemProperty -Path $regKey -Name $regName -ErrorAction SilentlyContinue) {
    Remove-ItemProperty -Path $regKey -Name $regName
    Write-Host "已移除开机自启动。" -ForegroundColor Green
} else {
    Write-Host "qtimer 未在开机自启动列表中。" -ForegroundColor Yellow
}
pause
