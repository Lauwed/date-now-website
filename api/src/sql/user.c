/**
 * @file user.c
 * @brief Postgres data-access implementation for the User table.
 *
 * email/totpSeed are encrypted at rest (AES-256-GCM, see lib/crypto.h).
 * Equality lookups by email go through emailHash (a deterministic
 * HMAC-SHA256 blind index) instead of the encrypted column. Substring
 * search and sort on email (get_users' `q`/implicit email sort) cannot be
 * pushed to Postgres on an encrypted column — the full user set is
 * fetched, decrypted, then filtered/sorted/paginated in this file. This
 * is an accepted tradeoff for a small user table (see migration plan).
 */

#include <ctype.h>
#include <enums.h>
#include <lib/crypto.h>
#include <lib/pg.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/user.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <structs.h>
#include <utils.h>

#define QUERY_COUNT_TMP "SELECT COUNT(*) FROM AppUser"
#define QUERY_EXISTS_TMP QUERY_COUNT_TMP " WHERE id = $1"
#define QUERY_SELECT_TMP                                                       \
  "SELECT "                                                                    \
  "u.id, u.username, u.email, u.role, u.link, "                               \
  "EXTRACT(EPOCH FROM u.subscribedAt)::BIGINT, "                              \
  "u.isSupporter, EXTRACT(EPOCH FROM u.createdAt)::BIGINT, "                  \
  "EXTRACT(EPOCH FROM u.trackerPixelConsentDate)::BIGINT, "                   \
  "m.id, m.textAlternatif, m.url, m.width, m.height "                        \
  "FROM AppUser u LEFT JOIN Media m ON m.id = u.picture"
#define QUERY_SELECT_SINGLE_TMP QUERY_SELECT_TMP " WHERE u.id = $1"
#define QUERY_SELECT_SINGLE_BY_EMAIL_TMP QUERY_SELECT_TMP " WHERE u.emailHash = $1"
#define QUERY_SELECT_SINGLE_TOTP_SEED                                          \
  "SELECT totpSeed FROM AppUser WHERE emailHash = $1;"

#define QUERY_POST_TMP                                                         \
  "INSERT INTO AppUser (username, email, emailHash, role, link, totpSeed, "    \
  "subscribedAt, trackerPixelConsentDate, picture) "                          \
  "VALUES ($1, $2, $3, COALESCE($4, 'USER'), $5, $6, TO_TIMESTAMP($7::BIGINT),"\
  " TO_TIMESTAMP($8::BIGINT), $9) RETURNING id;"
#define QUERY_PUT_TMP                                                          \
  "UPDATE AppUser "                                                            \
  "SET username = $1, email = $2, emailHash = $3, role = COALESCE($4, 'USER'),"\
  " link = $5, isSupporter = $6, totpSeed = $7, "                             \
  "trackerPixelConsentDate = TO_TIMESTAMP($8::BIGINT), picture = $9 "         \
  "WHERE id = $10;"

#define QUERY_DELETE_TMP "DELETE FROM AppUser WHERE id = $1;"

/* Lowercases into a new malloc'd string — email comparisons/hashes must be
 * case-insensitive regardless of how the user typed their address. */
static char *normalize_email(const char *email) {
  if (email == NULL)
    return NULL;
  char *out = strdup(email);
  for (char *p = out; *p; p++)
    *p = (char)tolower((unsigned char)*p);
  return out;
}

/* Case-insensitive substring search (used for the in-app `q` filter over
 * the decrypted user list — see file header). */
static int istrstr(const char *haystack, const char *needle) {
  if (haystack == NULL || needle == NULL)
    return 0;
  size_t hlen = strlen(haystack), nlen = strlen(needle);
  if (nlen == 0)
    return 1;
  if (nlen > hlen)
    return 0;
  for (size_t i = 0; i + nlen <= hlen; i++) {
    if (strncasecmp(haystack + i, needle, nlen) == 0)
      return 1;
  }
  return 0;
}

int user_exists(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== USER EXISTS SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_EXISTS_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_EXISTS_TMP, 1, values);
  if (res == NULL) {
    return -1;
  }

  int users_count = atoi(PQgetvalue(res, 0, 0));
  printf("COUNT:\t%d\n", users_count);

  PQclear(res);

  return users_count > 0;
}

int user_identity_exists(char *username, char *email) {
  if (email == NULL) {
    return -1;
  }

  printf(TERMINAL_SQL_MESSAGE("=== USER EMAIL EXISTS SQL ==="));

  char *normalized = normalize_email(email);
  char *email_hash = crypto_hmac_hex(normalized);
  free(normalized);

  const char *query =
      "SELECT COUNT(*) FROM AppUser WHERE username = $1 OR emailHash = $2;";
  const char *values[2] = {username, email_hash};
  GET_EXPANDED_QUERY(query, 2, values);

  PGresult *res = pg_exec(query, 2, values);
  free(email_hash);
  if (res == NULL) {
    return -1;
  }

  int users_count = atoi(PQgetvalue(res, 0, 0));
  printf("USERS COUNT:\t%d\n", users_count);

  PQclear(res);

  return users_count > 0;
}

/* Fetches every user row (no WHERE/ORDER/LIMIT — those can't be pushed to
 * Postgres for the email column once encrypted, see file header),
 * decrypting each user's email via user_map(). Caller must free_users(). */
static int fetch_all_users(struct user ***out, size_t *out_count) {
  const char *query = QUERY_SELECT_TMP ";";
  GET_EXPANDED_QUERY(query, 0, NULL);

  PGresult *res = pg_exec(query, 0, NULL);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  struct user **arr = n_rows > 0 ? malloc(n_rows * sizeof(struct user *)) : NULL;
  size_t count = 0;

  for (int i = 0; i < n_rows; i++) {
    struct user *u = malloc(sizeof(struct user));
    if (user_init(u) != 0) {
      free(u);
      continue;
    }

    struct media *m = malloc(sizeof(struct media));
    pg_row_t row = {res, i};
    if (user_map(u, &row, 0, 8) != 0) {
      free(m);
      free(u);
      continue;
    }

    if (media_map(m, &row, 9, 12) != 0) {
      free(m);
    } else {
      u->picture = m;
    }

    arr[count++] = u;
  }

  PQclear(res);

  *out = arr;
  *out_count = count;
  return 0;
}

static int user_matches_q(struct user *u, const char *q) {
  return istrstr(u->username, q) || istrstr(u->email, q) || istrstr(u->link, q);
}

static int email_cmp_asc(const void *a, const void *b) {
  struct user *const *ua = a, *const *ub = b;
  const char *ea = (*ua)->email ? (*ua)->email : "";
  const char *eb = (*ub)->email ? (*ub)->email : "";
  return strcasecmp(ea, eb);
}

static int email_cmp_desc(const void *a, const void *b) {
  return -email_cmp_asc(a, b);
}

int get_users_len(const struct mg_str *q) {
  printf(TERMINAL_SQL_MESSAGE("=== GET USERS COUNT SQL ==="));

  struct user **all = NULL;
  size_t all_count = 0;
  if (fetch_all_users(&all, &all_count) != 0) {
    return -1;
  }

  int matched = (int)all_count;
  if (q->len > 0) {
    char *q_str = malloc(q->len + 1);
    snprintf(q_str, q->len + 1, "%.*s", (int)q->len, q->buf);

    matched = 0;
    for (size_t i = 0; i < all_count; i++) {
      if (user_matches_q(all[i], q_str))
        matched++;
    }
    free(q_str);
  }

  if (all_count > 0) {
    free_users(all, all_count);
  }

  return matched;
}

int get_users_count(const char *type) {
  printf(TERMINAL_SQL_MESSAGE("=== GET USERS COUNT (filtered) SQL ==="));

  const char *where = NULL;
  if (type != NULL) {
    if (strcmp(type, "subscriber") == 0)
      where = " WHERE subscribedAt IS NOT NULL";
    else if (strcmp(type, "author") == 0)
      where = " WHERE role = 'AUTHOR'";
  }

  char query[128] = QUERY_COUNT_TMP;
  if (where)
    strcat(query, where);
  strcat(query, ";");

  GET_EXPANDED_QUERY(query, 0, NULL);

  PGresult *res = pg_exec(query, 0, NULL);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int users_count = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  return users_count;
}

int get_users(size_t len, struct user **arr, const struct mg_str *q,
              const struct mg_str *sort, int page, int page_size) {
  printf(TERMINAL_SQL_MESSAGE("=== GET USERS SQL ==="));

  const char *sort_keyword = "ASC";
  if (sort->len > 0) {
    if (strncasecmp(sort->buf, "desc", sort->len) == 0) {
      sort_keyword = "DESC";
    } else if (strncasecmp(sort->buf, "asc", sort->len) == 0) {
      sort_keyword = "ASC";
    } else {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("WRONG VALUE FOR SORTING"));
      return HTTP_BAD_REQUEST;
    }
  }

  struct user **all = NULL;
  size_t all_count = 0;
  if (fetch_all_users(&all, &all_count) != 0) {
    return HTTP_INTERNAL_ERROR;
  }

  // Filter by q (in-app — see file header)
  struct user **matched = all;
  size_t matched_count = all_count;
  if (q->len > 0) {
    char *q_str = malloc(q->len + 1);
    snprintf(q_str, q->len + 1, "%.*s", (int)q->len, q->buf);

    matched = malloc(all_count * sizeof(struct user *));
    matched_count = 0;
    for (size_t i = 0; i < all_count; i++) {
      if (user_matches_q(all[i], q_str)) {
        matched[matched_count++] = all[i];
      } else {
        free_user(all[i]);
      }
    }
    free(q_str);
    free(all);
  }

  // Sort by email (always — matches the original hardcoded ORDER BY email)
  qsort(matched, matched_count, sizeof(struct user *),
        strcmp(sort_keyword, "DESC") == 0 ? email_cmp_desc : email_cmp_asc);

  // Paginate in-app
  size_t start = 0, end = matched_count;
  if (page > 0) {
    start = (size_t)((page - 1) * page_size);
    end = start + (size_t)page_size;
    if (start > matched_count)
      start = matched_count;
    if (end > matched_count)
      end = matched_count;
  }

  size_t count = 0;
  for (size_t i = start; i < end && count < len; i++) {
    arr[count++] = matched[i];
  }
  // Free any matched users outside the returned page window
  for (size_t i = 0; i < matched_count; i++) {
    if (i < start || i >= end) {
      free_user(matched[i]);
    }
  }
  free(matched);

  return 0;
}

int get_user(struct user *user, int id) {
  if (id <= 0) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET USER SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int user_init_rc = user_init(user);
  if (user_init_rc != 0) {
    fprintf(stderr, "The user is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  struct media *m = malloc(sizeof(struct media));
  pg_row_t row = {res, 0};
  int user_rc = user_map(user, &row, 0, 8);
  if (user_rc != 0) {
    free(m);
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  int picture_rc = media_map(m, &row, 9, 12);
  if (picture_rc != 0) {
    free(m);
  } else {
    user->picture = m;
  }

  PQclear(res);

  return 0;
}

int get_user_by_email(struct user *user, char *email) {
  if (email == NULL) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET USER WITH EMAIL SQL ==="));

  char *normalized = normalize_email(email);
  char *email_hash = crypto_hmac_hex(normalized);
  free(normalized);

  const char *values[1] = {email_hash};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_BY_EMAIL_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_BY_EMAIL_TMP, 1, values);
  free(email_hash);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  int user_init_rc = user_init(user);
  if (user_init_rc != 0) {
    fprintf(stderr, "The user is NULL\n");
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  struct media *m = malloc(sizeof(struct media));
  pg_row_t row = {res, 0};
  int user_rc = user_map(user, &row, 0, 8);
  if (user_rc != 0) {
    free(m);
    PQclear(res);
    return HTTP_INTERNAL_ERROR;
  }

  int picture_rc = media_map(m, &row, 9, 12);
  if (picture_rc != 0) {
    free(m);
  } else {
    user->picture = m;
  }

  PQclear(res);

  return 0;
}

int get_user_totp_seed(char *email, char **seed) {
  if (email == NULL) {
    return HTTP_BAD_REQUEST;
  }

  printf(TERMINAL_SQL_MESSAGE("=== GET USER TOTP SEED ==="));

  char *normalized = normalize_email(email);
  char *email_hash = crypto_hmac_hex(normalized);
  free(normalized);

  const char *values[1] = {email_hash};
  GET_EXPANDED_QUERY(QUERY_SELECT_SINGLE_TOTP_SEED, 1, values);

  PGresult *res = pg_exec(QUERY_SELECT_SINGLE_TOTP_SEED, 1, values);
  free(email_hash);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  if (PQntuples(res) == 0 || PQgetisnull(res, 0, 0)) {
    PQclear(res);
    return HTTP_NOT_FOUND;
  }

  *seed = crypto_decrypt_hex(PQgetvalue(res, 0, 0));
  PQclear(res);

  return *seed == NULL;
}

int add_user(struct user *user) {
  printf(TERMINAL_SQL_MESSAGE("=== ADD USER SQL ==="));

  char *normalized = normalize_email(user->email);
  char *email_cipher = crypto_encrypt_hex(normalized);
  char *email_hash = crypto_hmac_hex(normalized);
  free(normalized);
  char *totp_cipher = crypto_encrypt_hex(user->totp_seed);

  char subscribed_at_str[16], tracker_str[16], picture_str[16];
  snprintf(subscribed_at_str, sizeof(subscribed_at_str), "%d",
           user->subscribed_at);
  snprintf(tracker_str, sizeof(tracker_str), "%d",
           user->tracker_pixel_consent_date);
  const char *picture_val = NULL;
  if (user->picture != NULL && user->picture->id > 0) {
    snprintf(picture_str, sizeof(picture_str), "%d", user->picture->id);
    picture_val = picture_str;
  }

  const char *values[9] = {user->username,   email_cipher, email_hash,
                           user->role,        user->link,   totp_cipher,
                           subscribed_at_str, tracker_str,  picture_val};
  GET_EXPANDED_QUERY(QUERY_POST_TMP, 9, values);

  PGresult *res = pg_exec(QUERY_POST_TMP, 9, values);
  free(email_cipher);
  free(email_hash);
  free(totp_cipher);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  user->id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return 0;
}

int edit_user(struct user *user) {
  printf(TERMINAL_SQL_MESSAGE("=== EDIT USER SQL ==="));

  char *normalized = normalize_email(user->email);
  char *email_cipher = crypto_encrypt_hex(normalized);
  char *email_hash = crypto_hmac_hex(normalized);
  free(normalized);
  char *totp_cipher = crypto_encrypt_hex(user->totp_seed);

  char is_supporter_str[8], tracker_str[16], picture_str[16], id_str[16];
  snprintf(is_supporter_str, sizeof(is_supporter_str), "%d",
           user->is_supporter);
  snprintf(tracker_str, sizeof(tracker_str), "%d",
           user->tracker_pixel_consent_date);
  snprintf(id_str, sizeof(id_str), "%d", user->id);
  const char *picture_val = NULL;
  if (user->picture != NULL && user->picture->id > 0) {
    snprintf(picture_str, sizeof(picture_str), "%d", user->picture->id);
    picture_val = picture_str;
  }

  const char *values[10] = {user->username, email_cipher,    email_hash,
                            user->role,      user->link,      is_supporter_str,
                            totp_cipher,     tracker_str,     picture_val,
                            id_str};
  GET_EXPANDED_QUERY(QUERY_PUT_TMP, 10, values);

  PGresult *res = pg_exec(QUERY_PUT_TMP, 10, values);
  free(email_cipher);
  free(email_hash);
  free(totp_cipher);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int delete_user(int id) {
  printf(TERMINAL_SQL_MESSAGE("=== DELETE USER SQL ==="));

  char id_str[16];
  snprintf(id_str, sizeof(id_str), "%d", id);
  const char *values[1] = {id_str};
  GET_EXPANDED_QUERY(QUERY_DELETE_TMP, 1, values);

  PGresult *res = pg_exec(QUERY_DELETE_TMP, 1, values);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  PQclear(res);

  return 0;
}

int get_subscriber_emails(size_t *len, char ***emails) {
  printf(TERMINAL_SQL_MESSAGE("=== GET SUBSCRIBER EMAILS SQL ==="));

  const char *query = "SELECT email FROM AppUser WHERE subscribedAt IS NOT NULL;";
  GET_EXPANDED_QUERY(query, 0, NULL);

  PGresult *res = pg_exec(query, 0, NULL);
  if (res == NULL) {
    return HTTP_INTERNAL_ERROR;
  }

  int n_rows = PQntuples(res);
  *len = (size_t)n_rows;
  if (n_rows == 0) {
    PQclear(res);
    *emails = NULL;
    return 0;
  }

  *emails = malloc((size_t)n_rows * sizeof(char *));
  for (int i = 0; i < n_rows; i++) {
    (*emails)[i] = crypto_decrypt_hex(PQgetvalue(res, i, 0));
  }

  PQclear(res);
  return 0;
}
