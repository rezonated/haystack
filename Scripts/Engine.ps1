# Copyright (c) 2026 Vanan Andreas.
#
# Dot-sourced by the scripts in this folder.
#
# Resolve-Engine turns -Engine into an engine root: a launcher version such as 5.8, a build name registered under
# HKCU\Software\Epic Games\Unreal Engine\Builds, or an engine directory. Empty means the .uproject's EngineAssociation.
# Build-Editor compiles the project's editor target with that engine, so a run after an engine switch does not stop
# at the module rebuild prompt.

function Resolve-Engine([string]$Engine, [string]$Project) {
    if (-not $Engine) { $Engine = (Get-Content $Project | ConvertFrom-Json).EngineAssociation }
    if (Test-Path "$Engine\Engine\Build\BatchFiles\RunUAT.bat") { return (Resolve-Path $Engine).Path }
    $root = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Engine" -ErrorAction SilentlyContinue).InstalledDirectory
    if (-not $root) { $root = (Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue).$Engine }
    if (-not $root) { throw "No engine '$Engine': not an engine directory, not a launcher version, not a registered build" }
    $root -replace '/', '\'
}

function Build-Editor([string]$EngineRoot, [string]$Project) {
    $target = [IO.Path]::GetFileNameWithoutExtension($Project) + "Editor"
    "Building $target with $EngineRoot"
    & "$EngineRoot\Engine\Build\BatchFiles\Build.bat" $target Win64 Development "-Project=`"$Project`"" -WaitMutex -NoHotReload |
        Select-String -Pattern 'error|Result:' | ForEach-Object { $_.Line }
    if ($LASTEXITCODE -ne 0) { throw "$target build failed with $EngineRoot" }
}
