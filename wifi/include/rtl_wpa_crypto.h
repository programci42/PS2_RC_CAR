/* SPDX-License-Identifier: MIT */
#ifndef RTL_WPA_CRYPTO_H
#define RTL_WPA_CRYPTO_H
#include <stddef.h>
#include <stdint.h>
/* Primitive helpers only, NOT a WPA2 supplicant/association state machine. */
int rtl_wpa_pmk(const uint8_t *password,size_t password_len,
                 const uint8_t *ssid,size_t ssid_len,uint8_t pmk[32]);
/* Nonces are inputs, never generated here. Caller must ensure fresh SNonce. */
int rtl_wpa_ptk(const uint8_t pmk[32],const uint8_t ap[6],const uint8_t sta[6],
                 const uint8_t anonce[32],const uint8_t snonce[32],uint8_t ptk[64]);
int rtl_wpa_eapol_mic(const uint8_t kck[16],const uint8_t *eapol,size_t len,uint8_t mic[16]);
int rtl_wpa_eapol_verify(const uint8_t kck[16],const uint8_t *eapol,size_t len);
int rtl_wpa_unwrap(const uint8_t kek[16],const uint8_t *wrapped,size_t len,
                    uint8_t *plain,size_t capacity,size_t *plain_len);
/* AES-CCM with the 13-byte nonce and 8-byte tag used by CCMP-128.
 * Caller supplies correctly constructed 802.11 AAD/nonce and tracks replay PN.
 * These functions alone do not implement 802.11 header framing or replay checks. */
int rtl_wpa_ccm_encrypt(const uint8_t tk[16],const uint8_t nonce[13],
    const uint8_t *aad,size_t aad_len,const uint8_t *plain,size_t len,uint8_t *cipher,uint8_t tag[8]);
int rtl_wpa_ccm_decrypt(const uint8_t tk[16],const uint8_t nonce[13],
    const uint8_t *aad,size_t aad_len,const uint8_t *cipher,size_t len,const uint8_t tag[8],uint8_t *plain);
#endif
