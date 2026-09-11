$env:PS2DEV = "C:/ps2dev"
$env:PS2SDK = "C:/ps2dev/ps2sdk"
$env:Path = "C:\msys64\usr\bin;C:\msys64\mingw32\bin;C:\ps2dev\bin;C:\ps2dev\ee\bin;C:\ps2dev\iop\bin;C:\ps2dev\dvp\bin;C:\ps2dev\ps2sdk\bin;C:\ps2dev\ps2sdk\ports\bin;" + $env:Path

make clean
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

make
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item -LiteralPath "relay.elf" -Destination "role.elf" -Force

# relay.elf hata ayıklama sembollerini geliştirici için korur. Hafıza kartına
# kopyalanan role.elf ise aynı LOAD segmenti ve giriş adresiyle, gereksiz DWARF
# bölümleri çıkarılmış dağıtım dosyasıdır. Bu hem kopyayı hızlandırır hem de
# uLaunchELF/hafıza kartı üzerinde gereksiz alan ve parçalanma baskısını azaltır.
& "C:\ps2dev\ee\bin\mips64r5900el-ps2-elf-strip.exe" --strip-debug "role.elf"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Updated stripped role.elf"
