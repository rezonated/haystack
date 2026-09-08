# Copyright (c) 2026 Vanan Andreas.

param(
    [string]$Config = "Development"
)

. $PSScriptRoot/SetupEnv.ps1

$buildTarget = "{0}Editor" -f $projectName

Write-Host "============================================="
Write-Host " BUILD GAME - $Config"
Write-Host " Project : $projectName"
Write-Host " Target  : $buildTarget"
Write-Host "============================================="

& $ubt -project="$uprojectPath" $buildTarget Win64 $Config `
    -WaitMutex `
    -FromMsBuild `
    -architecture=x64
