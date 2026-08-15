/**
 * @file crypto.c
 * @brief Field-level encryption for sensitive columns (User.email,
 *        User.totpSeed) — see include/lib/crypto.h for the design.
 */

#include <lib/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_LEN 32
#define NONCE_LEN 12
#define TAG_LEN 16

static unsigned char aes_key[KEY_LEN];
static unsigned char hmac_key[KEY_LEN];
static int crypto_ready = 0;

/* Minimal standard-alphabet base64 decoder (only used once, at startup, on
 * a short key string — no need for OpenSSL's BIO-based decoder). */
static int base64_decode(const char *in, unsigned char *out, size_t out_cap,
                         size_t *out_len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  size_t len = strlen(in);
  while (len > 0 && in[len - 1] == '=')
    len--;

  size_t pos = 0;
  int buf = 0, bits = 0;
  for (size_t i = 0; i < len; i++) {
    const char *p = strchr(alphabet, in[i]);
    if (p == NULL || in[i] == '\0')
      return -1;
    int v = (int)(p - alphabet);
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (pos >= out_cap)
        return -1;
      out[pos++] = (unsigned char)((buf >> bits) & 0xFF);
    }
  }

  *out_len = pos;
  return 0;
}

static void hex_encode(const unsigned char *in, size_t in_len, char *out) {
  static const char hex[] = "0123456789abcdef";
  out[0] = '\\';
  out[1] = 'x';
  for (size_t i = 0; i < in_len; i++) {
    out[2 + i * 2] = hex[(in[i] >> 4) & 0xF];
    out[3 + i * 2] = hex[in[i] & 0xF];
  }
  out[2 + in_len * 2] = '\0';
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/* Decodes a Postgres bytea text literal ("\xHEXHEX...") into raw bytes.
 * Returns a malloc'd buffer (caller frees) and sets *out_len, or NULL. */
static unsigned char *hex_decode(const char *in, size_t *out_len) {
  if (in[0] == '\\' && in[1] == 'x')
    in += 2;

  size_t len = strlen(in);
  if (len % 2 != 0)
    return NULL;

  unsigned char *out = malloc(len / 2);
  if (!out)
    return NULL;

  for (size_t i = 0; i < len / 2; i++) {
    int hi = hex_nibble(in[i * 2]);
    int lo = hex_nibble(in[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      free(out);
      return NULL;
    }
    out[i] = (unsigned char)((hi << 4) | lo);
  }

  *out_len = len / 2;
  return out;
}

int crypto_init(const char *master_key_b64) {
  if (master_key_b64 == NULL) {
    return -1;
  }

  unsigned char master_key[KEY_LEN];
  size_t decoded_len = 0;
  if (base64_decode(master_key_b64, master_key, sizeof(master_key),
                    &decoded_len) != 0 ||
      decoded_len != KEY_LEN) {
    return -1;
  }

  unsigned int len;
  const char *aes_ctx = "date-now-email-aes-key";
  const char *hmac_ctx = "date-now-email-hmac-key";
  HMAC(EVP_sha256(), master_key, KEY_LEN, (const unsigned char *)aes_ctx,
       strlen(aes_ctx), aes_key, &len);
  HMAC(EVP_sha256(), master_key, KEY_LEN, (const unsigned char *)hmac_ctx,
       strlen(hmac_ctx), hmac_key, &len);

  crypto_ready = 1;
  return 0;
}

char *crypto_encrypt_hex(const char *plaintext) {
  if (plaintext == NULL || !crypto_ready) {
    return NULL;
  }

  size_t plain_len = strlen(plaintext);
  unsigned char nonce[NONCE_LEN];
  if (RAND_bytes(nonce, NONCE_LEN) != 1) {
    return NULL;
  }

  unsigned char *ciphertext = malloc(plain_len > 0 ? plain_len : 1);
  int out_len = 0, final_len = 0;
  unsigned char tag[TAG_LEN];

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    free(ciphertext);
    return NULL;
  }

  int ok = 1;
  ok &= EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1;
  ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_LEN, NULL) == 1;
  ok &= EVP_EncryptInit_ex(ctx, NULL, NULL, aes_key, nonce) == 1;
  ok &= EVP_EncryptUpdate(ctx, ciphertext, &out_len,
                          (const unsigned char *)plaintext,
                          (int)plain_len) == 1;
  ok &= EVP_EncryptFinal_ex(ctx, ciphertext + out_len, &final_len) == 1;
  ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag) == 1;

  EVP_CIPHER_CTX_free(ctx);

  if (!ok) {
    free(ciphertext);
    return NULL;
  }

  size_t cipher_len = (size_t)(out_len + final_len);
  size_t packed_len = NONCE_LEN + cipher_len + TAG_LEN;
  unsigned char *packed = malloc(packed_len);
  memcpy(packed, nonce, NONCE_LEN);
  memcpy(packed + NONCE_LEN, ciphertext, cipher_len);
  memcpy(packed + NONCE_LEN + cipher_len, tag, TAG_LEN);
  free(ciphertext);

  char *hex = malloc(2 + packed_len * 2 + 1);
  hex_encode(packed, packed_len, hex);
  free(packed);

  return hex;
}

char *crypto_decrypt_hex(const char *hex_bytea) {
  if (hex_bytea == NULL || !crypto_ready) {
    return NULL;
  }

  size_t packed_len = 0;
  unsigned char *packed = hex_decode(hex_bytea, &packed_len);
  if (!packed || packed_len < NONCE_LEN + TAG_LEN) {
    free(packed);
    return NULL;
  }

  const unsigned char *nonce = packed;
  const unsigned char *ciphertext = packed + NONCE_LEN;
  size_t cipher_len = packed_len - NONCE_LEN - TAG_LEN;
  unsigned char tag[TAG_LEN];
  memcpy(tag, packed + NONCE_LEN + cipher_len, TAG_LEN);

  unsigned char *plaintext = malloc(cipher_len + 1);
  int out_len = 0, final_len = 0;

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    free(packed);
    free(plaintext);
    return NULL;
  }

  int ok = 1;
  ok &= EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1;
  ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_LEN, NULL) == 1;
  ok &= EVP_DecryptInit_ex(ctx, NULL, NULL, aes_key, nonce) == 1;
  ok &= EVP_DecryptUpdate(ctx, plaintext, &out_len, ciphertext,
                          (int)cipher_len) == 1;
  ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag) == 1;
  int verify_ok = EVP_DecryptFinal_ex(ctx, plaintext + out_len, &final_len);

  EVP_CIPHER_CTX_free(ctx);
  free(packed);

  if (!ok || verify_ok <= 0) {
    free(plaintext);
    return NULL;
  }

  plaintext[out_len + final_len] = '\0';
  return (char *)plaintext;
}

char *crypto_hmac_hex(const char *plaintext) {
  if (plaintext == NULL || !crypto_ready) {
    return NULL;
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  HMAC(EVP_sha256(), hmac_key, KEY_LEN, (const unsigned char *)plaintext,
       strlen(plaintext), digest, &digest_len);

  char *hex = malloc(2 + digest_len * 2 + 1);
  hex_encode(digest, digest_len, hex);

  return hex;
}
