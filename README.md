# qtimer

极简桌面小宠物计时器。一只程序化小猫在桌面底部溜达，顺便帮你管时间。

纯 Win32 实现，零依赖，单文件可执行。

---

## 功能

| 功能 | 说明 |
|------|------|
| **Coffee Time** | 阻止屏幕熄灭与系统休眠，不受电源策略影响 |
| **番茄钟** | 25 分钟专注 + 5 分钟休息，循环计时，底部圆点记录轮数 |
| **倒计时关机** | 15 / 30 / 60 / 120 分钟后自动关机 |
| **桌面小宠物** | 小猫在屏幕底部行走，自动转向，偶尔停下休息 |

---

## 使用方法

**启动**：直接双击 `qtimer.exe`，小猫出现在桌面底部。

**操作**：

- **右键** 小猫 → 打开菜单，选择模式或退出
- **左键拖动** → 移动位置
- 处于任意模式时右键 → 仅显示"停止"选项

**模式标识**：

| 状态 | 顶部文字颜色 |
|------|-------------|
| 空闲 | 灰色 `qtimer` |
| Coffee Time | 琥珀色 `coffee~`，猫头顶冒泡 |
| 番茄钟专注 | 红色 `work MM:SS` |
| 番茄钟休息 | 绿色 `break MM:SS` |
| 倒计时关机 | 红色 `off MM:SS` |

---

## 开机自启动

> 以下脚本操作的是当前用户注册表，**无需管理员权限**。

**添加自启动**：右键 `install-startup.ps1` → 使用 PowerShell 运行

**移除自启动**：右键 `uninstall-startup.ps1` → 使用 PowerShell 运行

如果系统提示执行策略限制，以管理员身份运行 PowerShell 执行一次：

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

---

## 构建

每次推送 `v*` tag 自动通过 GitHub Actions 编译，产物为 `qtimer.zip`，包含：

```
qtimer.exe
install-startup.ps1
uninstall-startup.ps1
README.md
```

手动构建（需要 MSVC）：

```cmd
cl /O2 /W3 /nologo qtimer.cpp /link /OUT:qtimer.exe /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib
```

---

## 系统要求

- Windows 10 / 11
- 无需安装任何运行库
