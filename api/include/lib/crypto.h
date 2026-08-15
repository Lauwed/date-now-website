#pragma once

/**
 * @file lib/crypto.h
 * @brief Field-level encryption for sensitive columns (User.email,
 *        User.totpSeed) stored as Postgres BYTEA.
 *
 * Two independent operations, both backed by a single master key read once
 * from the DB_ENCRYPTION_KEY env var:
 *
 * - **Reversible encryption** (crypto_encrypt_hex/crypto_decrypt_hex):
 *   AES-256-GCM, used for values that must be recovered in full (email for
 *   display/JWT claims, totpSeed for TOTP verification). Ciphertext is
 *   packed as `nonce(12) || ciphertext || tag(16)` and hex-encoded with a
 *   leading "\x" so the result is a valid Postgres bytea text literal,
 *   ready to bind as an ordinary text-format query parameter — no other
 *   file in the SQL layer needs to know these columns are binary.
 *
 * - **Blind index** (crypto_hmac_hex): HMAC-SHA256, deterministic and
 *   non-reversible, used for equality lookups on an encrypted column
 *   (User.emailHash) without ever storing or comparing plaintext at the
 *   DB level.
 *
 * The two operations use independently derived sub-keys (HMAC of the
 * master key with a fixed context string) so the same key material is
 * never reused for two different cryptographic purposes.
 */

#include <stddef.h>

/**
 * @brief Derives the AES and HMAC sub-keys from DB_ENCRYPTION_KEY (must be
 *        called once at startup, before any other crypto_* call).
 * @param master_key_b64 Base64-encoded 32-byte master key.
 * @return 0 on success, -1 if the key is missing/malformed.
 */
int crypto_init(const char *master_key_b64);

/**
 * @brief Encrypts a NUL-terminated string with AES-256-GCM.
 * @param plaintext String to encrypt, or NULL.
 * @return A malloc'd `"\xHEX..."` string (a valid Postgres bytea text
 *         literal) ready to bind as a query parameter, or NULL if
 *         @p plaintext is NULL. Caller must free().
 */
char *crypto_encrypt_hex(const char *plaintext);

/**
 * @brief Decrypts a value previously produced by crypto_encrypt_hex(), in
 *        the `"\xHEX..."` text form returned by Postgres for a bytea
 *        column.
 * @param hex_bytea Postgres bytea text output (leading "\x"), or NULL.
 * @return A malloc'd NUL-terminated plaintext string, or NULL if
 *         @p hex_bytea is NULL or decryption/authentication fails. Caller
 *         must free().
 */
char *crypto_decrypt_hex(const char *hex_bytea);

/**
 * @brief Computes a deterministic HMAC-SHA256 blind index of a string
 *        (used for User.emailHash equality lookups).
 * @param plaintext String to index, or NULL.
 * @return A malloc'd `"\xHEX..."` string (valid Postgres bytea text
 *         literal), or NULL if @p plaintext is NULL. Caller must free().
 */
char *crypto_hmac_hex(const char *plaintext);
