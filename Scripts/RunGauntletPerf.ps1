# Copyright (c) 2026 Vanan Andreas.
#
# Runs the Gauntlet idle performance test over a matrix of resolutions and configurations, then builds one
# PerfReportTool HTML report from every CSV capture and prints the summary files.
#
#   .\Scripts\RunGauntletPerf.ps1                                              # 1080p, 1440p, native, Development, editor build
#   .\Scripts\RunGauntletPerf.ps1 -Configurations Development,Shipping -Build "C:\Staged\Windows"
#   .\Scripts\RunGauntletPerf.ps1 -Resolutions 1920x1080 -Capture 30

param(
    [string[]]$Resolutions = @(),
    [string[]]$Configurations = @("Development"),
    [string]$Build = "editor",
    [int]$Warmup = 5,
    [int]$Capture = 60,
    [switch]$Fullscreen,
    [string]$OutDir = ""
)

$projectDir = Split-Path -Parent $PSScriptRoot
$proj = Get-ChildItem "$projectDir\*.uproject" | Select-Object -First 1 -ExpandProperty FullName
$assoc = (Get-Content $proj | ConvertFrom-Json).EngineAssociation
$engine = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc" -ErrorAction SilentlyContinue).InstalledDirectory
if (-not $engine) { $engine = (Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue).$assoc }

if ($Resolutions.Count -eq 0) {
    Add-Type -AssemblyName System.Windows.Forms
    $native = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $Resolutions = @("1920x1080", "2560x1440", "$($native.Width)x$($native.Height)") | Select-Object -Unique
}
if (-not $OutDir) { $OutDir = "$projectDir\Saved\Gauntlet\HayPerf_$(Get-Date -Format yyyyMMdd_HHmmss)" }
New-Item -ItemType Directory -Force $OutDir | Out-Null

foreach ($configuration in $Configurations) {
    foreach ($resolution in $Resolutions) {
        $resX, $resY = $resolution -split "x"
        "=== $configuration $resolution"
        $args = @(
            "-ScriptsForProject=`"$proj`"",
            "RunUnreal",
            "-project=`"$proj`"",
            "-platform=Win64",
            "-configuration=$configuration",
            "-build=$Build",
            "-test=HayPerf",
            "-HayResX=$resX",
            "-HayResY=$resY",
            "-HayPerfWarmup=$Warmup",
            "-HayPerfCapture=$Capture",
            "-HayPerfOut=`"$OutDir`"",
            "-log",
            "-logdir=`"$projectDir\Saved\Gauntlet`""
        )
        if ($Fullscreen) { $args += "-HayFullscreen" }
        & "$engine\Engine\Build\BatchFiles\RunUAT.bat" @args
    }
}

$csvs = Get-ChildItem "$OutDir\*.csv" -ErrorAction SilentlyContinue
if ($csvs) {
    & "$engine\Engine\Binaries\DotNET\CsvTools\PerfreportTool.exe" -csvdir "$OutDir" -o "$OutDir\Report" -summaryTableOutputFormats html,csv | Out-Null
    "Report: $OutDir\Report"
}

"=== Summaries"
Get-ChildItem "$OutDir\*.txt" | ForEach-Object { "--- $($_.Name)"; Get-Content $_.FullName }
