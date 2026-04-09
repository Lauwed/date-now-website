#include <lib/sqlite3.h>
#include <macros/colors.h>
#include <macros/sql.h>
#include <sql/auth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern sqlite3 *db;

int auth_get_user_by_email(const char *email, int *user_id,
                           char totp_seed[255]) {
  printf(TERMINAL_SQL_MESSAGE("=== AUTH GET USER BY EMAIL ==="));

  const char *query =
      "SELECT id, COALESCE(totpSeed, '') FROM User WHERE email = ? LIMIT 1;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
  GET_EXPANDED_QUERY(stmt);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return -1;
  }

  *user_id = sqlite3_column_int(stmt, 0);
  const char *seed = (const char *)sqlite3_column_text(stmt, 1);
  if (seed != NULL) {
    strncpy(totp_seed, seed, 254);
    totp_seed[254] = '\0';
  } else {
    totp_seed[0] = '\0';
  }

  sqlite3_finalize(stmt);
  return 0;
}

int auth_create_email_token(int user_id, const char *token_hash) {
  printf(TERMINAL_SQL_MESSAGE("=== AUTH CREATE EMAIL TOKEN ==="));

  /* Invalider les tokens précédents non utilisés */
  const char *invalidate_query =
      "UPDATE EmailToken SET used = 1 WHERE userId = ? AND used = 0;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, invalidate_query, -1, &stmt, NULL);
  if (rc == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  const char *query =
      "INSERT INTO EmailToken (userId, tokenHash, expiresAt) "
      "VALUES (?, ?, datetime('now', '+15 minutes'));";

  rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_int(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, token_hash, -1, SQLITE_STATIC);
  GET_EXPANDED_QUERY(stmt);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? 0 : -1;
}

int auth_verify_email_token(const char *email, const char *token_hash,
                            int *user_id) {
  printf(TERMINAL_SQL_MESSAGE("=== AUTH VERIFY EMAIL TOKEN ==="));

  const char *query =
      "SELECT et.id, et.userId FROM EmailToken et "
      "JOIN User u ON u.id = et.userId "
      "WHERE u.email = ? AND et.tokenHash = ? AND et.used = 0 "
      "AND datetime('now') < et.expiresAt "
      "LIMIT 1;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, token_hash, -1, SQLITE_STATIC);
  GET_EXPANDED_QUERY(stmt);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return -1;
  }

  int token_id = sqlite3_column_int(stmt, 0);
  *user_id = sqlite3_column_int(stmt, 1);
  sqlite3_finalize(stmt);

  /* Marquer comme utilisé */
  const char *use_query =
      "UPDATE EmailToken SET used = 1 WHERE id = ?;";
  rc = sqlite3_prepare_v2(db, use_query, -1, &stmt, NULL);
  if (rc == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, token_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  return 0;
}

int auth_create_session(int user_id, const char *token_hash) {
  printf(TERMINAL_SQL_MESSAGE("=== AUTH CREATE SESSION ==="));

  const char *query =
      "INSERT INTO AuthSession (userId, tokenHash, expiresAt) "
      "VALUES (?, ?, datetime('now', '+24 hours'));";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_int(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, token_hash, -1, SQLITE_STATIC);
  GET_EXPANDED_QUERY(stmt);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? 0 : -1;
}

int auth_verify_session(const char *token_hash) {
  printf(TERMINAL_SQL_MESSAGE("=== AUTH VERIFY SESSION ==="));

  const char *query =
      "SELECT userId FROM AuthSession "
      "WHERE tokenHash = ? AND revoked = 0 "
      "AND datetime('now') < expiresAt "
      "LIMIT 1;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, token_hash, -1, SQLITE_STATIC);
  GET_EXPANDED_QUERY(stmt);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return -1;
  }

  int uid = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return uid;
}

int auth_revoke_session(const char *token_hash) {
  printf(TERMINAL_SQL_MESSAGE("=== AUTH REVOKE SESSION ==="));

  const char *query =
      "UPDATE AuthSession SET revoked = 1 WHERE tokenHash = ?;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("prepare error: %s\n"),
            sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, token_hash, -1, SQLITE_STATIC);
  GET_EXPANDED_QUERY(stmt);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? 0 : -1;
}
