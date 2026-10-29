param([switch]$WifiFlowTests,[switch]$PhysicalNetworkTest)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$releaseVersion=([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'VERSION'))).Trim()
if($releaseVersion -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid VERSION'}
$versionHeader='/* Build version, shared by the screen and Release/Surum.txt. */'+"`n"+'#define WIFI_VERSION "'+$releaseVersion+'"'+"`n"
$versionHeaderPath=Join-Path $PSScriptRoot 'include/wifi_version.h'
if(!(Test-Path -LiteralPath $versionHeaderPath) -or [IO.File]::ReadAllText($versionHeaderPath) -ne $versionHeader){[IO.File]::WriteAllText($versionHeaderPath,$versionHeader)}
$firmware='third_party\realtek-firmware\rtl8192cfw.bin'
if((Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash -ne '787B6793E256B94F2295444D23436F327932AB8E79345DFFF0EBFFE73706C196'){
    throw 'Bundled Realtek firmware differs from the verified unmodified reference.'
}
$env:PS2DEV='C:/ps2dev'
$env:PS2SDK='C:/ps2dev/ps2sdk'
$env:Path='C:\msys64\usr\bin;C:\msys64\mingw32\bin;C:\ps2dev\bin;C:\ps2dev\ee\bin;C:\ps2dev\iop\bin;C:\ps2dev\ps2sdk\bin;'+$env:Path
& make
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
if($PhysicalNetworkTest){
    & make reports/physical-join-template.elf
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
}
if($WifiFlowTests){
    & make wifi-tour reports/wifi-pad-test.elf reports/wifi-pad-reject-test.elf
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
}
New-Item -ItemType Directory -Path dist -Force | Out-Null
foreach($name in 'rtl8192cu-probe.elf','rtl8192cu-selftest.elf','rtl8192cu-usbnet-test.elf','rtl8192cu-bringup.elf','rtl8192cu-boot-model-test.elf','rtl8192cu-wpa-crypto-test.elf','rtl8192cu-scan-parser-test.elf','rtl8192cu-wifi.elf','rtl8192cu-wifi-ui-preview.elf','rtl8192cu-wifi-flow-test.elf'){
    Copy-Item -LiteralPath $name -Destination (Join-Path dist $name) -Force
    & C:\ps2dev\ee\bin\mips64r5900el-ps2-elf-strip.exe --strip-debug (Join-Path dist $name)
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
}
New-Item -ItemType Directory -Path dist\licenses -Force | Out-Null
Copy-Item -LiteralPath third_party\realtek-firmware\Realtek-Firmware-License.txt -Destination dist\licenses\Realtek-Firmware-License.txt -Force
Copy-Item -LiteralPath third_party\mbedtls\LICENSE -Destination dist\licenses\MbedTLS-Apache-2.0.txt -Force
Copy-Item -LiteralPath third_party\urtwn-reference\LICENSE.txt -Destination dist\licenses\NetBSD-urtwn-ISC.txt -Force
Copy-Item -LiteralPath LICENSE -Destination dist\licenses\Project-MIT.txt -Force
Copy-Item -LiteralPath third_party\ps2sdk-usbd\LICENSE -Destination dist\licenses\PS2SDK-USBD-AFL-2.0.txt -Force
Copy-Item -LiteralPath third_party\urtwn-reference\TKIP-BSD-License.txt -Destination dist\licenses\TKIP-BSD-License.txt -Force
# Stable user-facing output; all future builds overwrite this one ELF.
New-Item -ItemType Directory -Path Release\licenses -Force | Out-Null
$seedPath=Join-Path $PSScriptRoot 'Release/Wifi.seed'
if(!(Test-Path -LiteralPath $seedPath)){
    # Private first-install data only. Never regenerate an existing source.
    $seedBytes=New-Object byte[] 64
    $seedRng=[Security.Cryptography.RandomNumberGenerator]::Create()
    try {$seedRng.GetBytes($seedBytes);[IO.File]::WriteAllBytes($seedPath,$seedBytes)}
    finally {$seedRng.Dispose();[Array]::Clear($seedBytes,0,$seedBytes.Length)}
}
Copy-Item -LiteralPath dist\rtl8192cu-wifi.elf -Destination Release\Wifi.elf -Force
Copy-Item dist\licenses\* Release\licenses -Force
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'Release/Surum.txt'),$releaseVersion+"`r`n")
Copy-Item -LiteralPath RELEASE_README_EN.txt -Destination Release\README.txt -Force
$knownPath=Join-Path $PSScriptRoot 'Release/bilinenaglar.csv'
if(!(Test-Path -LiteralPath $knownPath)){[IO.File]::WriteAllText($knownPath,"SSID;Sifre`r`n",[Text.Encoding]::UTF8)}
Get-FileHash dist\*.elf -Algorithm SHA256 | Format-Table -AutoSize
