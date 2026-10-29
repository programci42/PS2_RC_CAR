$pcsx2 = "C:\ps2dev\emulators\pcsx2\pcsx2-qt.exe"
$iso = Join-Path $PSScriptRoot "role-disc.iso"
$log = Join-Path $PSScriptRoot "pcsx2-run.log"

# OpenPS2BIOS loads files from a disc, not PCSX2's host: ELF path.
& $pcsx2 -portable -batch $iso -logfile $log
