/*
 * TOTP (RFC 6238) — Time-Based One-Time Password
 *
 * Implémentation autonome de SHA-1, HMAC-SHA1, Base32 et TOTP.
 * Aucune dépendance externe.
 */

#include <lib/totp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ SHA-1 */

#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_SIZE 20

typedef struct {
  uint32_t h[5];
  uint64_t bit_count;
  uint8_t buf[SHA1_BLOCK_SIZE];
  uint32_t buf_len;
} sha1_ctx;

static uint32_t rot32(uint32_t x, int n) {
  return (x << n) | (x >> (32 - n));
}

static void sha1_process_block(sha1_ctx *ctx, const uint8_t block[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
  }
  for (int i = 16; i < 80; i++) {
    w[i] = rot32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3],
           e = ctx->h[4];

  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t tmp = rot32(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rot32(b, 30);
    b = a;
    a = tmp;
  }

  ctx->h[0] += a;
  ctx->h[1] += b;
  ctx->h[2] += c;
  ctx->h[3] += d;
  ctx->h[4] += e;
}

static void sha1_init(sha1_ctx *ctx) {
  ctx->h[0] = 0x67452301;
  ctx->h[1] = 0xEFCDAB89;
  ctx->h[2] = 0x98BADCFE;
  ctx->h[3] = 0x10325476;
  ctx->h[4] = 0xC3D2E1F0;
  ctx->bit_count = 0;
  ctx->buf_len = 0;
}

static void sha1_update(sha1_ctx *ctx, const uint8_t *data, size_t len) {
  ctx->bit_count += (uint64_t)len * 8;
  for (size_t i = 0; i < len; i++) {
    ctx->buf[ctx->buf_len++] = data[i];
    if (ctx->buf_len == SHA1_BLOCK_SIZE) {
      sha1_process_block(ctx, ctx->buf);
      ctx->buf_len = 0;
    }
  }
}

static void sha1_final(sha1_ctx *ctx, uint8_t digest[SHA1_DIGEST_SIZE]) {
  uint64_t bit_count = ctx->bit_count;
  ctx->buf[ctx->buf_len++] = 0x80;

  if (ctx->buf_len > 56) {
    while (ctx->buf_len < 64)
      ctx->buf[ctx->buf_len++] = 0;
    sha1_process_block(ctx, ctx->buf);
    ctx->buf_len = 0;
  }
  while (ctx->buf_len < 56)
    ctx->buf[ctx->buf_len++] = 0;

  /* Longueur en big-endian sur 8 octets */
  for (int i = 7; i >= 0; i--) {
    ctx->buf[56 + i] = (uint8_t)(bit_count & 0xff);
    bit_count >>= 8;
  }
  sha1_process_block(ctx, ctx->buf);

  for (int i = 0; i < 5; i++) {
    digest[i * 4] = (ctx->h[i] >> 24) & 0xff;
    digest[i * 4 + 1] = (ctx->h[i] >> 16) & 0xff;
    digest[i * 4 + 2] = (ctx->h[i] >> 8) & 0xff;
    digest[i * 4 + 3] = ctx->h[i] & 0xff;
  }
}

/* --------------------------------------------------------------- HMAC-SHA1 */

static void hmac_sha1(const uint8_t *key, size_t key_len,
                      const uint8_t *data, size_t data_len,
                      uint8_t result[SHA1_DIGEST_SIZE]) {
  uint8_t k[SHA1_BLOCK_SIZE];
  memset(k, 0, sizeof(k));

  if (key_len > SHA1_BLOCK_SIZE) {
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, key, key_len);
    sha1_final(&ctx, k);
  } else {
    memcpy(k, key, key_len);
  }

  uint8_t ipad[SHA1_BLOCK_SIZE], opad[SHA1_BLOCK_SIZE];
  for (int i = 0; i < SHA1_BLOCK_SIZE; i++) {
    ipad[i] = k[i] ^ 0x36;
    opad[i] = k[i] ^ 0x5c;
  }

  uint8_t inner[SHA1_DIGEST_SIZE];
  sha1_ctx ctx;
  sha1_init(&ctx);
  sha1_update(&ctx, ipad, SHA1_BLOCK_SIZE);
  sha1_update(&ctx, data, data_len);
  sha1_final(&ctx, inner);

  sha1_init(&ctx);
  sha1_update(&ctx, opad, SHA1_BLOCK_SIZE);
  sha1_update(&ctx, inner, SHA1_DIGEST_SIZE);
  sha1_final(&ctx, result);
}

/* ----------------------------------------------------------------- Base32 */

static int b32_char_value(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a';
  if (c >= '2' && c <= '7')
    return c - '2' + 26;
  return -1;
}

int base32_decode(const char *input, unsigned char *output, int out_len) {
  int buf = 0, bits = 0, out_pos = 0;
  for (int i = 0; input[i] != '\0' && input[i] != '='; i++) {
    int v = b32_char_value(input[i]);
    if (v < 0)
      continue;
    buf = (buf << 5) | v;
    bits += 5;
    if (bits >= 8) {
      if (out_pos >= out_len)
        return -1;
      output[out_pos++] = (unsigned char)((buf >> (bits - 8)) & 0xff);
      bits -= 8;
    }
  }
  return out_pos;
}

/* -------------------------------------------------------------------- TOTP */

int totp_generate(const char *base32_secret, time_t ts, uint32_t *otp) {
  uint8_t key[64];
  int key_len = base32_decode(base32_secret, key, sizeof(key));
  if (key_len <= 0)
    return -1;

  uint64_t T = (uint64_t)ts / 30;

  uint8_t msg[8];
  for (int i = 7; i >= 0; i--) {
    msg[i] = (uint8_t)(T & 0xff);
    T >>= 8;
  }

  uint8_t digest[SHA1_DIGEST_SIZE];
  hmac_sha1(key, key_len, msg, 8, digest);

  int offset = digest[SHA1_DIGEST_SIZE - 1] & 0x0f;
  uint32_t code = ((uint32_t)(digest[offset] & 0x7f) << 24) |
                  ((uint32_t)digest[offset + 1] << 16) |
                  ((uint32_t)digest[offset + 2] << 8) |
                  (uint32_t)digest[offset + 3];

  *otp = code % 1000000;
  return 0;
}

int totp_verify(const char *base32_secret, const char *code, int window) {
  uint32_t expected;
  time_t now = time(NULL);
  int code_int = atoi(code);

  for (int delta = -window; delta <= window; delta++) {
    time_t ts = now + (time_t)(delta * 30);
    if (totp_generate(base32_secret, ts, &expected) != 0)
      return 0;
    if ((int)expected == code_int)
      return 1;
  }
  return 0;
}
