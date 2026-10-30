# Local reset recovery changes

Upstream pinned in UPSTREAM.json. Original Academic Free License 2.0 is retained.

- Bound a USB port reset to two seconds; release the global reset token on missing/failing/busy completion and retry that port after one second.
- Rearm external hub interrupt status polling after a failed reset.
- Cancel reset watchdogs before USB address assignment and on ordinary unplug cleanup.
- Preserve subsequent timer absolute deadlines when cancelling a timer in the delta list.
- Exported USBD ABI and power-budget checks remain unchanged.

- Match external hub children by their one-based attached port number (upstream lookup advanced one child too far).
- Inspect every pre-connected port after power setup, independently of interrupt change bits; keep the initial scan cursor in reserved m_pad3[0].
- Include hub bit zero when sizing interrupt bitmaps, so port 8 is not omitted; clamp child count to the configured and bitmap limits.
# Runtime hub recovery (0.4.32)

Hub/port/interrupt status completion errors used to return without scheduling
another request. Runtime error callbacks now retry with 250 ms to 4 s backoff,
rescan ports after a lost change bitmap and respect pending DMA ownership.
Only a connected, disabled child port is re-enumerated; enabled ports,
overcurrent ports and an active reset are preserved. Disconnect cancels the
hub's retry timer. No automatic whole-hub power cycle is performed.
The private hub structure gained a timer; all USBD objects depend on its header.

