#pragma once

/**
 * @file totp.h
 * @brief TOTP code generation and verification (RFC 6238) and secret generation.
 */

#include <stdint.h>
#include <time.h>

/**
 * @brief Generates the current TOTP code for a given base32 secret.
 *
 * @param base32_secret Base32-encoded secret (NUL-terminated string).
 * @param step_seconds  Time step duration (typically 30 seconds).
 * @return 6-digit TOTP code (e.g. 123456).
 * @note @p base32_secret is not freed by this function.
 */
uint32_t totp_generate(const char *base32_secret, uint32_t step_seconds);

/**
 * @brief Generates the TOTP code for a specific timestamp (for testing or
 *        sliding-window validation).
 *
 * Allows validating codes from the previous and next time steps to
 * compensate for minor clock skew.
 *
 * @param base32_secret Base32-encoded secret.
 * @param step_seconds  Time step duration in seconds.
 * @param timestamp     Unix timestamp for which to generate the code.
 * @return 6-digit TOTP code.
 * @note @p base32_secret is not freed by this function.
 */
uint32_t totp_generate_at(const char *base32_secret, uint32_t step_seconds,
                          time_t timestamp);

/**
 * @brief Generates a new random TOTP secret encoded in base32.
 *
 * Uses OpenSSL (RAND_bytes) to generate 20 random bytes and encodes them
 * in base32 into the output buffer.
 *
 * @param output Output buffer of at least 64 bytes (matches the
 *               @c totp_seed field of struct user).
 * @return 0 on success, non-zero on OpenSSL error.
 * @note @p output is filled but not allocated by this function.
 */
int totp_generate_secret(char *output);
