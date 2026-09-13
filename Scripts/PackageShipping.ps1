# Copyright (c) 2026 Vanan Andreas.
#
# Cooks, stages and packages the Shipping game into Packaged\Windows, drops the launch .bat files and README.txt from
# Scripts\Packaged next to the executable, and zips the folder for hand-out.
#
#   .\Scripts\PackageShipping.ps1                       # engine from the .uproject association
#   .\Scripts\PackageShipping.ps1 -Engine VA-UE582-src  # the source engine, whose Shipping keeps console, log and CSV
#   .\Scripts\PackageShipping.ps1 -NoZip

param(
    [string]$Engine = "",
    [switch]$NoZip
)

. "$PSScriptRoot\Engine.ps1"
$projectDir = Split-Path -Parent $PSScriptRoot
$proj = Get-ChildItem "$projectDir\*.uproject" | Select-Object -First 1 -ExpandProperty FullName
$engine = Resolve-Engine $Engine $proj
$archiveDir = "$projectDir\Packaged"
$package = "$archiveDir\Windows"

"=== Packaging Shipping with $engine"
# -nodebuginfo keeps the 1 GB Shipping .pdb out of the package, the editor's IncludeDebugFiles setting does not reach UAT
# from here. The app-local directory puts the 4 MB of VC runtime DLLs next to the executable instead of the 29 MB of
# redistributable installers that -prereqs would add.
& "$engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=`"$proj`"" -platform=Win64 -clientconfig=Shipping `
    -build -cook -stage -pak -nodebuginfo "-applocaldirectory=`"$engine\Engine\Binaries\ThirdParty\AppLocalDependencies`"" `
    -package -archive "-archivedirectory=`"$archiveDir`"" -unattended -nop4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item "$PSScriptRoot\Packaged\*" $package -Force
"Package: $package"

if (-not $NoZip) {
    # Deflate at 7-Zip's ultra settings, so Explorer can still open the archive. The paks are Oodle compressed already,
    # the gain is on the executable and the loose DLLs.
    $zip = "$archiveDir\Haystack-Shipping-$(Get-Date -Format yyyyMMdd).zip"
    if (Test-Path $zip) { Remove-Item $zip }
    & 7z a -tzip -mx=9 -mfb=258 -mpass=15 -bso0 -bsp0 $zip "$package\*"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    "Zip: $zip, $([int]((Get-Item $zip).Length / 1MB)) MB"
}
