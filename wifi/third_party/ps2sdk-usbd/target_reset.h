/* Called with the USBD lock held. Never reset a hub or another device. */
static int resetMatchedDevice(int id, unsigned vid, unsigned pid)
{
    UsbdDevice_t *dev=fetchDeviceById(id);
    UsbDeviceDescriptor *desc;
    if(!dev || !dev->m_parent || dev->m_childListStart ||
       dev->m_deviceStatus!=DEVICE_READY) return USB_RC_BADDEV;
    desc=(UsbDeviceDescriptor*)dev->m_staticDeviceDescPtr;
    if(!desc || desc->bLength<18 || desc->bDescriptorType!=1 ||
       desc->bDeviceClass==9 || desc->idVendor!=vid || desc->idProduct!=pid)
        return USB_RC_BADDEV;
    if(dev->m_parent==memPool->m_deviceTreeRoot){
        if(dev->m_attachedToPortNo<1 || dev->m_attachedToPortNo>2 ||
           (memPool->m_ohciRegs->HcRhPortStatus[dev->m_attachedToPortNo-1]&BIT(PORT_OVER_CURRENT)))
            return USB_RC_BADDEV;
    }
    flushPort(dev);
    dev->m_deviceStatus=DEVICE_CONNECTED;
    addTimerCallback(&dev->m_timer,(TimerCallback)hubResetDevice,dev,501);
    return 0;
}
