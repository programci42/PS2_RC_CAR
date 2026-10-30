/* Low-speed compatibility: settle after reset and read the first eight
 * descriptor bytes at address zero before SET_ADDRESS. Retain the global
 * reset token until addressing completes; never probe two default devices. */
static void lowSpeedDescriptorDone(UsbdIoRequest_t *req)
{
    UsbdDevice_t *dev=req->m_correspEndpoint->m_correspDevice;
    const unsigned char *d=dev->m_staticDeviceDescPtr;
    if(dev->m_deviceStatus!=DEVICE_RESETCOMPLETE)return;
    if(req->m_resultCode || req->m_transferedBytes<8 || d[0]<18 || d[1]!=1 || d[7]!=8){
        usbdTraceEnumeration(dev,5,req->m_resultCode?req->m_resultCode:255);
        killDevice(dev,req->m_correspEndpoint);
        return;
    }
    addTimerCallback(&dev->m_timer,hubSetFuncAddress,req->m_correspEndpoint,21);
}
static void lowSpeedDescriptorStart(void *arg)
{
    UsbdEndpoint_t *ep=(UsbdEndpoint_t *)arg;
    UsbdDevice_t *dev=ep->m_correspDevice;
    if(!dev || dev->m_deviceStatus!=DEVICE_RESETCOMPLETE)return;
    doControlTransfer(ep,&dev->m_ioRequest,USB_DIR_IN|USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR,USB_DT_DEVICE<<8,0,8,
        dev->m_staticDeviceDescPtr,lowSpeedDescriptorDone);
}
