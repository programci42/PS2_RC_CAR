$pcsx2 = "C:\ps2dev\emulators\pcsx2\pcsx2-qt.exe"
$elf = "C:\ps2dev\projeler\role\relay.elf"
$log = "C:\ps2dev\projeler\role\pcsx2-run.log"

& $pcsx2 -portable -batch -elf $elf -logfile $log
