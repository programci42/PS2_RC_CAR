using System;
using usbGenericHidCommunications;

namespace SedaRelayTester
{
    public sealed class SedaRelayHid : usbGenericHidCommunication
    {
        private const int SedaVidDecimal = 6030;
        private const int SedaPidDecimal = 2009;

        public SedaRelayHid()
            : base(SedaVidDecimal, SedaPidDecimal)
        {
        }

        public bool IsOpen
        {
            get { return isDeviceAttached; }
        }

        public bool Open(out string message)
        {
            findTargetDevice();

            if (isDeviceAttached)
            {
                message = "VID=6030 PID=2009 Baglandi (usbGenericHidCommunications.dll)";
                return true;
            }

            message = "Seda HID kart bulunamadi. Bu surum senin calisan programindaki usbGenericHidCommunications.dll ile ariyor.";
            return false;
        }

        public void Close()
        {
        }

        public bool SendSedaCommand(byte command, out string detail)
        {
            byte[] outputBuffer = new byte[9];

            outputBuffer[0] = 0;
            outputBuffer[1] = 1;
            outputBuffer[4] = command;

            bool success = writeRawReportToDevice(outputBuffer);
            detail = success ? "writeRawReportToDevice ok; len=9 idx=4" : "writeRawReportToDevice failed";
            return success;
        }
    }
}
