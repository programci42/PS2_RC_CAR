/* Minimal WPA2 primitive build; no TLS, network, filesystem or entropy source.
 * Nonces MUST come from a separately provisioned secure random source. */
#ifndef RTL_MBEDTLS_CONFIG_H
#define RTL_MBEDTLS_CONFIG_H
#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CCM_C
#define MBEDTLS_NIST_KW_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_PLATFORM_C
#endif
