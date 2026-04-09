#include <endpoints/auth.h>
#include <lib/mongoose.h>
#include <lib/totp.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/auth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <time.h>
#include <utils.h>

/* Adresse email expéditrice (peut être configurée via -DAUTH_FROM_EMAIL=...) */
#ifndef AUTH_FROM_EMAIL
#define AUTH_FROM_EMAIL "no-reply@date-now.local"
#endif

/* SHA1 hex digest (20 octets → 40 caractères + '\0') */
#define SHA1_HEX_LEN 41

/* ----------------------------------------------------------- SHA1 interne */
/* Déclarations minimales reprises de totp.c via linkage statique */

typedef struct {
  uint32_t h[5];
  uint64_t bit_count;
  uint8_t buf[64];
  uint32_t buf_len;
} _sha1_ctx;

static uint32_t _rot32(uint32_t x, int n) {
  return (x << n) | (x >> (32 - n));
}
static void _sha1_block(_sha1_ctx *ctx, const uint8_t block[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++)
    w[i] = ((uint32_t)block[i * 4] << 24) |
           ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
  for (int i = 16; i < 80; i++)
    w[i] = _rot32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

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
    uint32_t tmp = _rot32(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = _rot32(b, 30);
    b = a;
    a = tmp;
  }
  ctx->h[0] += a;
  ctx->h[1] += b;
  ctx->h[2] += c;
  ctx->h[3] += d;
  ctx->h[4] += e;
}
static void _sha1_init(_sha1_ctx *ctx) {
  ctx->h[0] = 0x67452301;
  ctx->h[1] = 0xEFCDAB89;
  ctx->h[2] = 0x98BADCFE;
  ctx->h[3] = 0x10325476;
  ctx->h[4] = 0xC3D2E1F0;
  ctx->bit_count = 0;
  ctx->buf_len = 0;
}
static void _sha1_update(_sha1_ctx *ctx, const uint8_t *data, size_t len) {
  ctx->bit_count += (uint64_t)len * 8;
  for (size_t i = 0; i < len; i++) {
    ctx->buf[ctx->buf_len++] = data[i];
    if (ctx->buf_len == 64) {
      _sha1_block(ctx, ctx->buf);
      ctx->buf_len = 0;
    }
  }
}
static void _sha1_final(_sha1_ctx *ctx, uint8_t digest[20]) {
  uint64_t bc = ctx->bit_count;
  ctx->buf[ctx->buf_len++] = 0x80;
  if (ctx->buf_len > 56) {
    while (ctx->buf_len < 64)
      ctx->buf[ctx->buf_len++] = 0;
    _sha1_block(ctx, ctx->buf);
    ctx->buf_len = 0;
  }
  while (ctx->buf_len < 56)
    ctx->buf[ctx->buf_len++] = 0;
  for (int i = 7; i >= 0; i--) {
    ctx->buf[56 + i] = (uint8_t)(bc & 0xff);
    bc >>= 8;
  }
  _sha1_block(ctx, ctx->buf);
  for (int i = 0; i < 5; i++) {
    digest[i * 4] = (ctx->h[i] >> 24) & 0xff;
    digest[i * 4 + 1] = (ctx->h[i] >> 16) & 0xff;
    digest[i * 4 + 2] = (ctx->h[i] >> 8) & 0xff;
    digest[i * 4 + 3] = ctx->h[i] & 0xff;
  }
}

/* Hash SHA1 d'une chaîne → hex string (40 chars + '\0') */
static void sha1_hex(const char *input, char out[SHA1_HEX_LEN]) {
  _sha1_ctx ctx;
  _sha1_init(&ctx);
  _sha1_update(&ctx, (const uint8_t *)input, strlen(input));
  uint8_t digest[20];
  _sha1_final(&ctx, digest);
  for (int i = 0; i < 20; i++)
    sprintf(out + i * 2, "%02x", digest[i]);
  out[40] = '\0';
}

/* ----------------------------------------- Génération de tokens aléatoires */

/* Lit n octets depuis /dev/urandom et les encode en hexadécimal. */
static int random_hex(char *out, int n_bytes) {
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f)
    return -1;
  uint8_t *buf = malloc(n_bytes);
  if (fread(buf, 1, n_bytes, f) != (size_t)n_bytes) {
    fclose(f);
    free(buf);
    return -1;
  }
  fclose(f);
  for (int i = 0; i < n_bytes; i++)
    sprintf(out + i * 2, "%02x", buf[i]);
  out[n_bytes * 2] = '\0';
  free(buf);
  return 0;
}

/* Génère un code email à 6 chiffres depuis /dev/urandom. */
static int random_6digit(char out[7]) {
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f)
    return -1;
  uint32_t val;
  if (fread(&val, sizeof(val), 1, f) != 1) {
    fclose(f);
    return -1;
  }
  fclose(f);
  snprintf(out, 7, "%06u", val % 1000000);
  return 0;
}

/* ---------------------------------------------------- Envoi d'email simple */

static void send_email(const char *to, const char *subject, const char *body) {
  FILE *fp = popen("sendmail -t 2>/dev/null", "w");
  if (!fp) {
    fprintf(stderr,
            TERMINAL_ERROR_MESSAGE("sendmail non disponible sur ce système"));
    return;
  }
  fprintf(fp, "From: %s\r\n", AUTH_FROM_EMAIL);
  fprintf(fp, "To: %s\r\n", to);
  fprintf(fp, "Subject: %s\r\n", subject);
  fprintf(fp, "MIME-Version: 1.0\r\n");
  fprintf(fp, "Content-Type: text/plain; charset=UTF-8\r\n");
  fprintf(fp, "\r\n");
  fprintf(fp, "%s\r\n", body);
  pclose(fp);
}

/* ------------------------------------------------------------ check_auth() */

int check_auth(struct mg_http_message *msg) {
  struct mg_str *auth_header = mg_http_get_header(msg, "Authorization");
  if (auth_header == NULL || auth_header->len == 0)
    return -1;

  /* Format attendu : "Bearer <token>" */
  const char *prefix = "Bearer ";
  size_t prefix_len = strlen(prefix);
  if (auth_header->len <= prefix_len ||
      strncmp(auth_header->buf, prefix, prefix_len) != 0)
    return -1;

  /* Extraire le token brut */
  char token[256];
  size_t tok_len = auth_header->len - prefix_len;
  if (tok_len >= sizeof(token))
    return -1;
  strncpy(token, auth_header->buf + prefix_len, tok_len);
  token[tok_len] = '\0';

  /* Hasher et vérifier en DB */
  char token_hash[SHA1_HEX_LEN];
  sha1_hex(token, token_hash);

  int user_id = auth_verify_session(token_hash);
  return (user_id > 0) ? 0 : -1;
}

/* ---------------------------------------- POST /api/auth/request-token */

void send_auth_request_res(struct mg_connection *c,
                           struct mg_http_message *msg,
                           struct error_reply *error_reply) {
  error_reply = malloc(sizeof(struct error_reply));

  if (!mg_match(msg->method, mg_str("POST"), NULL)) {
    ERROR_REPLY_405;
    free(error_reply);
    return;
  }

  if (msg->body.len == 0) {
    ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
    free(error_reply);
    return;
  }

  /* Extraire l'email */
  int offset, length;
  offset = mg_json_get(msg->body, "$.email", &length);
  if (offset < 0) {
    ERROR_REPLY_400("L'email est requis.");
    free(error_reply);
    return;
  }
  char email[512];
  int email_len = length - 2;
  if (email_len <= 0 || email_len >= (int)sizeof(email)) {
    ERROR_REPLY_400("Email invalide.");
    free(error_reply);
    return;
  }
  strncpy(email, msg->body.buf + offset + 1, email_len);
  email[email_len] = '\0';

  /* Vérifier que l'utilisateur existe */
  int user_id;
  char totp_seed[255];
  if (auth_get_user_by_email(email, &user_id, totp_seed) != 0) {
    /* On retourne quand même 200 pour ne pas révéler l'existence du compte */
    mg_http_reply(c, 200, JSON_HEADER,
                  "{\"message\":\"Si ce compte existe, un email a été envoyé.\"}");
    free(error_reply);
    return;
  }

  /* Générer le code à 6 chiffres */
  char code[7];
  if (random_6digit(code) != 0) {
    ERROR_REPLY_500;
    free(error_reply);
    return;
  }

  /* Hasher et stocker */
  char token_hash[SHA1_HEX_LEN];
  sha1_hex(code, token_hash);

  if (auth_create_email_token(user_id, token_hash) != 0) {
    ERROR_REPLY_500;
    free(error_reply);
    return;
  }

  /* Envoyer le mail */
  char body[256];
  snprintf(body, sizeof(body),
           "Bonjour,\n\nVotre code de connexion Date.Now() : %s\n\n"
           "Ce code expire dans 15 minutes.\n\n"
           "Si vous n'avez pas demandé ce code, ignorez cet email.",
           code);
  send_email(email, "[Date.Now()] Code de connexion", body);

  printf(TERMINAL_SUCCESS_MESSAGE("=== AUTH REQUEST TOKEN SENT ==="));
  mg_http_reply(c, 200, JSON_HEADER,
                "{\"message\":\"Si ce compte existe, un email a été envoyé.\"}");
  free(error_reply);
}

/* ---------------------------------------- POST /api/auth/verify */

void send_auth_verify_res(struct mg_connection *c, struct mg_http_message *msg,
                          struct error_reply *error_reply) {
  error_reply = malloc(sizeof(struct error_reply));

  if (!mg_match(msg->method, mg_str("POST"), NULL)) {
    ERROR_REPLY_405;
    free(error_reply);
    return;
  }

  if (msg->body.len == 0) {
    ERROR_REPLY_400(BODY_REQUIRED_MESSAGE);
    free(error_reply);
    return;
  }

  int offset, length;

  /* Email */
  offset = mg_json_get(msg->body, "$.email", &length);
  if (offset < 0) {
    ERROR_REPLY_400("L'email est requis.");
    free(error_reply);
    return;
  }
  char email[512];
  int email_len = length - 2;
  if (email_len <= 0 || email_len >= (int)sizeof(email)) {
    ERROR_REPLY_400("Email invalide.");
    free(error_reply);
    return;
  }
  strncpy(email, msg->body.buf + offset + 1, email_len);
  email[email_len] = '\0';

  /* Email token (le code à 6 chiffres saisi par l'utilisateur) */
  offset = mg_json_get(msg->body, "$.emailToken", &length);
  if (offset < 0) {
    ERROR_REPLY_400("emailToken est requis.");
    free(error_reply);
    return;
  }
  char email_token[64];
  int et_len = length - 2;
  if (et_len <= 0 || et_len >= (int)sizeof(email_token)) {
    ERROR_REPLY_400("emailToken invalide.");
    free(error_reply);
    return;
  }
  strncpy(email_token, msg->body.buf + offset + 1, et_len);
  email_token[et_len] = '\0';

  /* Code TOTP */
  offset = mg_json_get(msg->body, "$.totp", &length);
  if (offset < 0) {
    ERROR_REPLY_400("totp est requis.");
    free(error_reply);
    return;
  }
  char totp_code[16];
  int totp_len = length - 2;
  if (totp_len <= 0 || totp_len >= (int)sizeof(totp_code)) {
    ERROR_REPLY_400("Code TOTP invalide.");
    free(error_reply);
    return;
  }
  strncpy(totp_code, msg->body.buf + offset + 1, totp_len);
  totp_code[totp_len] = '\0';

  /* Vérifier le token email */
  char email_token_hash[SHA1_HEX_LEN];
  sha1_hex(email_token, email_token_hash);

  int user_id;
  if (auth_verify_email_token(email, email_token_hash, &user_id) != 0) {
    mg_http_reply(c, 401, JSON_HEADER,
                  "{\"code\":401,\"message\":\"Code email invalide ou expiré.\"}");
    free(error_reply);
    return;
  }

  /* Récupérer le seed TOTP de l'utilisateur */
  int uid2;
  char totp_seed[255];
  if (auth_get_user_by_email(email, &uid2, totp_seed) != 0 ||
      totp_seed[0] == '\0') {
    mg_http_reply(
        c, 401, JSON_HEADER,
        "{\"code\":401,\"message\":\"TOTP non configuré pour ce compte.\"}");
    free(error_reply);
    return;
  }

  /* Vérifier le code TOTP (fenêtre ±1 période de 30s) */
  if (!totp_verify(totp_seed, totp_code, 1)) {
    mg_http_reply(c, 401, JSON_HEADER,
                  "{\"code\":401,\"message\":\"Code TOTP invalide.\"}");
    free(error_reply);
    return;
  }

  /* Générer le token de session (32 octets aléatoires → 64 chars hex) */
  char session_token[65];
  if (random_hex(session_token, 32) != 0) {
    ERROR_REPLY_500;
    free(error_reply);
    return;
  }

  /* Stocker le hash du token en DB */
  char session_hash[SHA1_HEX_LEN];
  sha1_hex(session_token, session_hash);

  if (auth_create_session(user_id, session_hash) != 0) {
    ERROR_REPLY_500;
    free(error_reply);
    return;
  }

  printf(TERMINAL_SUCCESS_MESSAGE("=== AUTH SESSION CREATED ==="));

  size_t resp_len =
      snprintf(NULL, 0, "{\"token\":\"%s\"}", session_token) + 1;
  char *resp = malloc(resp_len);
  sprintf(resp, "{\"token\":\"%s\"}", session_token);
  mg_http_reply(c, 200, JSON_HEADER, "%s\n", resp);
  free(resp);
  free(error_reply);
}

/* ---------------------------------------- DELETE /api/auth/session */

void send_auth_session_res(struct mg_connection *c,
                           struct mg_http_message *msg,
                           struct error_reply *error_reply) {
  error_reply = malloc(sizeof(struct error_reply));

  if (!mg_match(msg->method, mg_str("DELETE"), NULL)) {
    ERROR_REPLY_405;
    free(error_reply);
    return;
  }

  struct mg_str *auth_header = mg_http_get_header(msg, "Authorization");
  if (auth_header == NULL || auth_header->len == 0) {
    mg_http_reply(c, 401, JSON_HEADER,
                  "{\"code\":401,\"message\":\"Unauthorized\"}");
    free(error_reply);
    return;
  }

  const char *prefix = "Bearer ";
  size_t prefix_len = strlen(prefix);
  if (auth_header->len <= prefix_len ||
      strncmp(auth_header->buf, prefix, prefix_len) != 0) {
    mg_http_reply(c, 401, JSON_HEADER,
                  "{\"code\":401,\"message\":\"Unauthorized\"}");
    free(error_reply);
    return;
  }

  char token[256];
  size_t tok_len = auth_header->len - prefix_len;
  if (tok_len >= sizeof(token)) {
    mg_http_reply(c, 401, JSON_HEADER,
                  "{\"code\":401,\"message\":\"Unauthorized\"}");
    free(error_reply);
    return;
  }
  strncpy(token, auth_header->buf + prefix_len, tok_len);
  token[tok_len] = '\0';

  char token_hash[SHA1_HEX_LEN];
  sha1_hex(token, token_hash);
  auth_revoke_session(token_hash);

  printf(TERMINAL_SUCCESS_MESSAGE("=== AUTH SESSION REVOKED ==="));
  mg_http_reply(c, 200, JSON_HEADER,
                "{\"message\":\"Session révoquée avec succès.\"}");
  free(error_reply);
}
