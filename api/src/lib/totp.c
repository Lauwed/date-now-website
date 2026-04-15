#include <lib/totp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static int base32_char(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a';
  if (c >= '2' && c <= '7')
    return c - '2' + 26;
  return -1;
}

static int base32_decode(const char *input, uint8_t *output) {
  int len = strlen(input);
  int out_len = 0;
  uint32_t buf = 0;
  int bits = 0;

  for (int i = 0; i < len; i++) {
    int val = base32_char(input[i]);
    if (val < 0)
      continue;

    buf = (buf << 5) | val;
    bits += 5;

    if (bits >= 8) {
      output[out_len++] = (buf >> (bits - 8)) & 0xFF;
      bits -= 8;
    }
  }
  return out_len;
}

static void base32_encode(const uint8_t *input, int len, char *output) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int i = 0, out = 0;
  uint32_t buf = 0;
  int bits = 0;

  while (i < len) {
    buf = (buf << 8) | input[i++];
    bits += 8;
    while (bits >= 5) {
      output[out++] = alphabet[(buf >> (bits - 5)) & 0x1F];
      bits -= 5;
    }
  }
  if (bits > 0)
    output[out++] = alphabet[(buf << (5 - bits)) & 0x1F];

  output[out] = '\0';
}

uint32_t totp_generate(const char *base32_secret, uint32_t step_seconds) {
  uint8_t secret[64];
  int secret_len = base32_decode(base32_secret, secret);

  // Time counter
  uint64_t counter = (uint64_t)time(NULL) / step_seconds;

  // ???
  const size_t msg_len = 8;
  uint8_t msg[msg_len];
  for (int i = msg_len - 1; i >= 0; i--) {
    msg[i] = counter & 0xFF;
    counter >>= msg_len;
  }

  // HMAC
  uint8_t hash[20];
  unsigned int hash_len = 20;
  HMAC(EVP_sha1(), secret, secret_len, msg, msg_len, hash, &hash_len);

  int offset = hash[19] & 0x0F;
  uint32_t code = ((hash[offset] & 0x7F) << 24) |
                  ((hash[offset + 1] & 0xFF) << 16) |
                  ((hash[offset + 2] & 0xFF) << 8) | (hash[offset + 3] & 0xFF);

  return code % 1000000;
}

int totp_generate_secret(char *output) {
  uint8_t raw[20]; // 160 bits
  if (RAND_bytes(raw, sizeof(raw)) != 1)
    return -1;
  base32_encode(raw, sizeof(raw), output);
  return 0;
}
