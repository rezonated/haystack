# Copyright (c) 2026 Vanan Andreas.
#
# Runs the Gauntlet idle performance test over a matrix of resolutions and configurations, then builds one
# PerfReportTool HTML report from every CSV capture and prints the summary files.
#
#   .\Scripts\RunGauntletPerf.ps1                                              # 1080p, 1440p, native, Development, editor build
#   .\Scripts\RunGauntletPerf.ps1 -Configurations Development,Shipping         # cooks and stages each configuration first
#   .\Scripts\RunGauntletPerf.ps1 -Configurations Shipping -Build "C:\Staged"  # an existing staged build
#   .\Scripts\RunGauntletPerf.ps1 -Resolutions 1920x1080 -Capture 30
#   .\Scripts\RunGauntletPerf.ps1 -Engine 5.8                                  # launcher engine instead of the .uproject's
#
# Shipping needs a source engine: Haystack.Target.cs turns on logging, the console and the CSV profiler there,
# which an installed engine cannot compile into its precompiled Shipping modules.

param(
    [string[]]$Resolutions = @(),
    [string[]]$Configurations = @("Development"),
    [string]$Build = "editor",
    [switch]$Stage,
    [int]$Warmup = 5,
    [int]$Capture = 60,
    [switch]$Fullscreen,
    [string]$OutDir = "",
    [string]$Engine = ""
)

. "$PSScriptRoot\Engine.ps1"
$projectDir = Split-Path -Parent $PSScriptRoot
$proj = Get-ChildItem "$projectDir\*.uproject" | Select-Object -First 1 -ExpandProperty FullName
$engine = Resolve-Engine $Engine $proj
if ((Test-Path "$engine\Engine\Build\InstalledBuild.txt") -and ($Configurations | Where-Object { $_ -ne "Development" })) {
    Write-Error "$engine is an installed engine: its Shipping and Test builds have no log, console or CSV profiler, so Gauntlet cannot drive them. Use -Engine with the source engine."
    exit 1
}
Build-Editor $engine $proj

if ($Resolutions.Count -eq 0) {
    Add-Type -AssemblyName System.Windows.Forms
    $native = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $Resolutions = @("1920x1080", "2560x1440", "$($native.Width)x$($native.Height)") | Select-Object -Unique
}
if (-not $OutDir) { $OutDir = "$projectDir\Saved\Results\HayPerf_$(Get-Date -Format yyyyMMdd_HHmmss)" }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# The editor binary only runs Development. Any other configuration is cooked and staged into its own folder,
# which Gauntlet then picks up through -build.
if ($Build -eq "editor" -and ($Configurations | Where-Object { $_ -ne "Development" })) { $Stage = $true }

foreach ($configuration in $Configurations) {
    $configurationBuild = $Build
    if ($Stage) {
        $configurationBuild = "$projectDir\Saved\StagedBuilds\$configuration"
        "=== Staging $configuration to $configurationBuild"
        & "$engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=`"$proj`"" -platform=Win64 `
            "-clientconfig=$configuration" -build -cook -stage -pak -unattended -nop4 "-stagingdirectory=`"$configurationBuild`""
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    foreach ($resolution in $Resolutions) {
        $resX, $resY = $resolution -split "x"
        "=== $configuration $resolution"
        $args = @(
            "-ScriptsForProject=`"$proj`"",
            "RunUnreal",
            "-project=`"$proj`"",
            "-platform=Win64",
            "-configuration=$configuration",
            "-build=`"$configurationBuild`"",
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
