param(
    [switch]$ScreenTour,
    [ValidateSet('TR','EN')][string]$Language = 'TR',
    [ValidateRange(1,9)][int]$Page = 4
)
$ErrorActionPreference = 'Stop'
$emulator = 'C:\ps2dev\emulators\play\Play.exe'
if ($ScreenTour) {
    $elf = Join-Path $PSScriptRoot ('tests\ui-tour\TOUR_{0}_{1:00}.ELF' -f $Language,$Page)
    if (!(Test-Path -LiteralPath $elf)) {
        & node (Join-Path $PSScriptRoot 'tests\build-ui-tour.cjs') --build-all
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
} else {
    $elf = Join-Path $PSScriptRoot 'role.elf'
}
& $emulator --elf $elf
