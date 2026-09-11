$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $root "bin"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$csc = "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe"
$exe = Join-Path $out "SedaRelayTester.exe"
$hidDll = Join-Path $root "usbGenericHidCommunications.dll"

& $csc /nologo /target:winexe /platform:x86 /out:$exe `
    /reference:System.dll `
    /reference:System.Core.dll `
    /reference:System.Drawing.dll `
    /reference:System.Windows.Forms.dll `
    /reference:$hidDll `
    (Join-Path $root "Program.cs") `
    (Join-Path $root "MainForm.cs") `
    (Join-Path $root "SedaRelayHid.cs")

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item -LiteralPath $hidDll -Destination $out -Force

Write-Output $exe
