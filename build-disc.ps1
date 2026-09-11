$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$disc = Join-Path $root "disc"
$iso = Join-Path $root "role-disc.iso"
$bash = "C:\msys64\usr\bin\bash.exe"

powershell -ExecutionPolicy Bypass -File (Join-Path $root "build.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Force -Path $disc | Out-Null
Copy-Item -LiteralPath (Join-Path $root "role.elf") -Destination (Join-Path $disc "ROLE.ELF") -Force

if (!(Test-Path $bash)) {
    Write-Error "MSYS2 bash not found: $bash"
    exit 1
}

& $bash -lc "cd /c/ps2dev/projeler/role/disc && xorriso -as mkisofs -o /c/ps2dev/projeler/role/role-disc.iso -V ROLE_RELAY -sysid PLAYSTATION -iso-level 1 -l SYSTEM.CNF ROLE.ELF"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Updated $iso"
