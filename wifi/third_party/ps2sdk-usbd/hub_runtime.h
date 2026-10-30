/* Runtime hub recovery. Timer callbacks never cycle power or reset the hub.
 * Retry status/port discovery after a transient transfer error, retaining
 * ownership of pending transfers. Backoff is capped at four seconds. */
static void getHubStatusChange(UsbdUsbHub_t *hub);
static void hubRuntimeError(UsbdUsbHub_t *hub);
static void hubRuntimeHealthy(UsbdUsbHub_t *hub)
{
    hub->m_runtimeRetryCount = 0;
    if (hub->m_runtimeRetryTimer.m_isActive) cancelTimerCallback(&hub->m_runtimeRetryTimer);
}
static void hubRuntimeRetry(void *arg)
{
    UsbdUsbHub_t *hub = (UsbdUsbHub_t *)arg;
    if (!hub->m_dev || hub->m_dev->m_privDataField != hub) return;
    if (hub->m_controlIoReq.m_busyFlag || hub->m_statusIoReq.m_busyFlag) {
        hubRuntimeError(hub);
        return;
    }
    /* The last interrupt may have failed before a change bitmap arrived.
       Re-read every port, including already-connected children. */
    bzero(hub->m_statusChangeInfo, sizeof(hub->m_statusChangeInfo));
    hub->m_statusIoReq.m_resultCode = USB_RC_OK;
    hub->m_hubStatusCounter = 0;
    hub->m_pad3[0] = 1;
    getHubStatusChange(hub);
}
static void hubRuntimeError(UsbdUsbHub_t *hub)
{
    unsigned delay;
    if (!hub->m_dev || !hub->m_numChildDevices || hub->m_runtimeRetryTimer.m_isActive) return;
    delay = 250u << (hub->m_runtimeRetryCount < 4 ? hub->m_runtimeRetryCount : 4);
    if (hub->m_runtimeRetryCount < 4) hub->m_runtimeRetryCount++;
    addTimerCallback(&hub->m_runtimeRetryTimer, hubRuntimeRetry, hub, delay);
}
/* A connected child whose hub port lost ENABLE cannot answer USB traffic.
   Re-enumerate that child only; leave other ports and their LDDs intact. */
static int hubRuntimePortDisabled(UsbdUsbHub_t *hub, UsbdDevice_t *port)
{
    u32 status = hub->m_portStatusChange;
    if (!port || !(status & BIT(PORT_CONNECTION)) || (status & BIT(PORT_ENABLE)) ||
        (status & (BIT(PORT_RESET) | BIT(PORT_OVER_CURRENT))) ||
        port->m_deviceStatus < DEVICE_RESETCOMPLETE) return 0;
    flushPort(port);
    port->m_deviceStatus = DEVICE_CONNECTED;
    addTimerCallback(&port->m_timer, (TimerCallback)hubResetDevice, port, 501);
    return 1;
}
