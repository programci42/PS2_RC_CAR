/* Local hub port helpers. USB port numbers start at one, not zero. */
#ifndef USBD_LOCAL_HUB_PORTS_H
#define USBD_LOCAL_HUB_PORTS_H
static UsbdDevice_t *hubPortDevice(UsbdDevice_t *hub, int port)
{
    UsbdDevice_t *child;
    if (!hub || port < 1) return NULL;
    for (child=hub->m_childListStart;child;child=child->m_next)
        if (child->m_attachedToPortNo==(unsigned)port) return child;
    return NULL;
}
static unsigned hubStatusBytes(unsigned ports) { return (ports+8u)>>3; }
#endif
