/* SPDX-License-Identifier: MIT */
#ifndef RTL_NONCE_H
#define RTL_NONCE_H
#include <stdint.h>
enum {RTL_NONCE_MISSING=-40,RTL_NONCE_READ=-41,RTL_NONCE_INVALID=-42,
      RTL_NONCE_WRITE=-43,RTL_NONCE_VERIFY=-44,RTL_NONCE_CRYPTO=-45,
      RTL_NONCE_NO_CARD=-46,RTL_NONCE_UNFORMATTED=-47,RTL_NONCE_NO_SOURCE=-48,
      RTL_NONCE_IO_BUSY=-49};
typedef struct {
    void *context;
    int (*read)(void *,uint8_t out[64]);
    int (*write)(void *,const uint8_t data[64]);
    /* Must never truncate or overwrite an old file. */
    int (*create)(void *,const uint8_t data[64]);
} RtlNonceIo;
int rtl_nonce_next_io(const RtlNonceIo *,uint8_t nonce[32]);
int rtl_nonce_setup(const RtlNonceIo *target,const RtlNonceIo *source,uint8_t nonce[32]);
const char *rtl_nonce_error(int);
/* A unique 64-byte CSPRNG-generated secret must be provisioned at path.
 * Ratchets and verifies durable state BEFORE releasing a nonce. If storage
 * fails no connection is attempted. Never restore/clone an old seed file. */
int rtl_nonce_next(const char *path,uint8_t nonce[32]);
#endif
