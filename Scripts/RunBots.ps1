# Copyright (c) 2026 Vanan Andreas.
#
# Launches a listen server and one client of this project, both running the dig bot and the net stats logger,
# side by side in small windows, then prints the interesting log lines when the run ends.
#
#   .\Scripts\RunBots.ps1                      # random spots, 150 s
#   .\Scripts\RunBots.ps1 -BotArgs needle      # dig at the needle
#   .\Scripts\RunBots.ps1 -RunSeconds 60 -MaxFps 30
#   .\Scripts\RunBots.ps1 -Engine 5.8               # launcher engine instead of the .uproject's

param(
    [int]$RunSeconds = 150,
    [string]$BotArgs = "on",
    [string]$ExtraArgs = "",
    [int]$MaxFps = 60,
    [int]$ResX = 960,
    [int]$ResY = 540,
    [string]$Engine = ""
)

. "$PSScriptRoot\Engine.ps1"
$projectDir = Split-Path -Parent $PSScriptRoot
$proj = Get-ChildItem "$projectDir\*.uproject" | Select-Object -First 1 -ExpandProperty FullName
$engine = Resolve-Engine $Engine $proj
Build-Editor $engine $proj
$exe = "$engine\Engine\Binaries\Win64\UnrealEditor.exe"
$map = "/Game/ThirdPersonBP/Maps/ThirdPersonExampleMap"
$logs = "$projectDir\Saved\Logs"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class Win32Window {
    delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    public static List<IntPtr> WindowsOf(uint pid) {
        var result = new List<IntPtr>();
        EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) result.Add(h); return true; }, IntPtr.Zero);
        return result;
    }
}
'@

# Two windows side by side at the top left of the primary display's working area.
$area = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$w = $ResX + 16
$h = $ResY + 48
$slots = @(@($area.X, $area.Y), @(($area.X + $w + 20), $area.Y))
"Display work area $($area.Width)x$($area.Height) at $($area.X),$($area.Y), game windows ${ResX}x${ResY}"

# The engine runs -ExecCmds once per map load, so the bot gets "on" or "needle", never a bare toggle.
$exec = "t.MaxFPS $MaxFps,r.VSync 0,t.IdleWhenNotForeground 0,Hay.NetStats 5,Hay.DigBot $BotArgs"
$common = "-game -log -windowed -ResX=$ResX -ResY=$ResY $ExtraArgs -ExecCmds=`"$exec`""

# The engine recreates and centers its window a few seconds after launch, so keep pinning for a while.
function Pin-Windows($pairs, $seconds) {
    for ($t = 0; $t -lt $seconds * 2; $t++) {
        foreach ($pair in $pairs) {
            $proc = $pair[0]; $slot = $pair[1]
            foreach ($hwnd in [Win32Window]::WindowsOf([uint32]$proc.Id)) {
                # SWP_NOZORDER | SWP_SHOWWINDOW
                [Win32Window]::SetWindowPos($hwnd, [IntPtr]::Zero, $slot[0], $slot[1], $w, $h, 0x0044) | Out-Null
            }
        }
        Start-Sleep -Milliseconds 500
    }
}

Remove-Item "$logs\Bots_*.log" -ErrorAction SilentlyContinue

$server = Start-Process -FilePath $exe -ArgumentList "`"$proj`" ${map}?listen $common LOG=Bots_Server.log" -PassThru
Pin-Windows @(, @($server, $slots[0])) 15

$client = Start-Process -FilePath $exe -ArgumentList "`"$proj`" 127.0.0.1 $common LOG=Bots_Client1.log" -PassThru
Pin-Windows @(@($server, $slots[0]), @($client, $slots[1])) 25

Start-Sleep -Seconds $RunSeconds

Stop-Process -Id $client.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 3
Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

function Show-Log($path, $title) {
    "=== $title"
    Select-String -Path $path -Pattern "LogHay: (Warning: )?(Needle|Spawn done|DigBot: (needle|\d+ takes|on|off))|Join succeeded|Welcomed|GPU crash|Fatal" | Select-Object -ExpandProperty Line | Select-Object -Last 8
    Select-String -Path $path -Pattern "NetStats:" | Select-Object -ExpandProperty Line
}
Show-Log "$logs\Bots_Server.log" "Server"
Show-Log "$logs\Bots_Client1.log" "Client"
