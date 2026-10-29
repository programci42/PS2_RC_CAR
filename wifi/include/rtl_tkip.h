/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef RTL_TKIP_H
#define RTL_TKIP_H
#include <stddef.h>
#include <stdint.h>
/* GTK in AP wire order: TK[16], AP TX Michael[8], AP RX Michael[8].
 * Receive group frames only; pairwise traffic remains AES/CCMP.
 * No plaintext or replay-counter change until ICV AND Michael validate.
 * -5 is an authentic ICV but invalid Michael MIC (countermeasure event). */
int rtl_tkip_group_rx(const uint8_t gtk[32],const uint8_t ap[6],unsigned keyid,
    uint64_t replay[17],const uint8_t *frame,size_t length,uint8_t *plain,size_t capacity);
#endif
