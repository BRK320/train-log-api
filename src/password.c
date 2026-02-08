#include "password.h"
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#define SALT_LEN 16
#define DK_LEN   32
#define ITER     100000

static int hex_encode(const unsigned char *in, size_t in_len, char *out, size_t out_sz) {
  static const char *hex = "0123456789abcdef";
  if (out_sz < in_len * 2 + 1) return 0;
  for (size_t i = 0; i < in_len; i++) {
    out[i*2]     = hex[(in[i] >> 4) & 0xF];
    out[i*2 + 1] = hex[in[i] & 0xF];
  }
  out[in_len*2] = '\0';
  return 1;
}

static int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static int hex_decode(const char *in, unsigned char *out, size_t out_len) {
  size_t in_len = strlen(in);
  if (in_len != out_len * 2) return 0;
  for (size_t i = 0; i < out_len; i++) {
    int hi = hex_val(in[i*2]);
    int lo = hex_val(in[i*2+1]);
    if (hi < 0 || lo < 0) return 0;
    out[i] = (unsigned char)((hi << 4) | lo);
  }
  return 1;
}

int pwd_is_pbkdf2(const char *stored) {
  return stored && strncmp(stored, "pbkdf2$sha256$", 13) == 0;
}

static int pbkdf2_sha256(const char *password,
                         const unsigned char *salt, size_t salt_len,
                         unsigned char *dk, size_t dk_len) {
  BCRYPT_ALG_HANDLE hAlg = NULL;
  NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL,
                                            BCRYPT_ALG_HANDLE_HMAC_FLAG);
  if (st != 0) return 0;

  // PBKDF2 do Windows 
  st = BCryptDeriveKeyPBKDF2(
    hAlg,
    (PUCHAR) password, (ULONG) strlen(password),
    (PUCHAR) salt, (ULONG) salt_len,
    ITER,
    (PUCHAR) dk, (ULONG) dk_len,
    0
  );

  BCryptCloseAlgorithmProvider(hAlg, 0);
  return (st == 0);
}

int pwd_hash(const char *password, char *out, size_t out_size) {
  if (!password || !out || out_size == 0) return 0;

  unsigned char salt[SALT_LEN];
  unsigned char dk[DK_LEN];

  NTSTATUS st = BCryptGenRandom(NULL, salt, (ULONG)sizeof(salt),
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (st != 0) return 0;

  if (!pbkdf2_sha256(password, salt, sizeof(salt), dk, sizeof(dk))) return 0;

  char salt_hex[SALT_LEN*2 + 1];
  char dk_hex[DK_LEN*2 + 1];
  if (!hex_encode(salt, sizeof(salt), salt_hex, sizeof(salt_hex))) return 0;
  if (!hex_encode(dk, sizeof(dk), dk_hex, sizeof(dk_hex))) return 0;

  int n = snprintf(out, out_size, "pbkdf2$sha256$%d$%s$%s", ITER, salt_hex, dk_hex);
  return (n > 0 && (size_t)n < out_size);
}

int pwd_verify(const char *password, const char *stored) {
  if (!password || !stored) return 0;

  // Formato: pbkdf2$sha256$100000$<salt_hex>$<dk_hex>
  if (!pwd_is_pbkdf2(stored)) return 0;

  const char *p = stored;

  // salt começa depois do 3º '$'
  // pbkdf2$sha256$ITER$salt$dk
  const char *d1 = strchr(p, '$'); if (!d1) return 0;
  const char *d2 = strchr(d1 + 1, '$'); if (!d2) return 0;
  const char *d3 = strchr(d2 + 1, '$'); if (!d3) return 0;
  const char *d4 = strchr(d3 + 1, '$'); if (!d4) return 0;

  char salt_hex[64];
  char dk_hex[128];

  size_t salt_len = (size_t)(d4 - (d3 + 1));
  size_t dk_len = strlen(d4 + 1);

  if (salt_len != SALT_LEN * 2) return 0;
  if (dk_len != DK_LEN * 2) return 0;

  memcpy(salt_hex, d3 + 1, salt_len);
  salt_hex[salt_len] = '\0';
  strncpy(dk_hex, d4 + 1, sizeof(dk_hex) - 1);
  dk_hex[sizeof(dk_hex) - 1] = '\0';

  unsigned char salt[SALT_LEN];
  unsigned char dk_expected[DK_LEN];
  unsigned char dk_calc[DK_LEN];

  if (!hex_decode(salt_hex, salt, sizeof(salt))) return 0;
  if (!hex_decode(dk_hex, dk_expected, sizeof(dk_expected))) return 0;

  if (!pbkdf2_sha256(password, salt, sizeof(salt), dk_calc, sizeof(dk_calc))) return 0;

  // comparação constante 
  unsigned char diff = 0;
  for (size_t i = 0; i < sizeof(dk_calc); i++) diff |= (dk_calc[i] ^ dk_expected[i]);

  return diff == 0;
}
