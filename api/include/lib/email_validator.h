#pragma once

/**
 * @file lib/email_validator.h
 * @brief Email domain normalisation and DNS reachability checks.
 *
 * These functions are pure utilities with no SQLite dependency.
 * The admission orchestration (DNS + blocklist + app domain) lives
 * in utils.c via email_admission_inspect().
 */

/* Reason codes stored in email_admission_result.reason_code. */
#define EMAIL_ADMISSION_OK         0  /**< Accepted, no issue. */
#define EMAIL_ADMISSION_DNS_FAIL   1  /**< Domain has no MX / A / AAAA record. */
#define EMAIL_ADMISSION_BLOCKED    2  /**< Domain is in BlockedEmailDomain table. */
#define EMAIL_ADMISSION_APP_DOMAIN 3  /**< Domain matches APP_DOMAIN env var. */

/**
 * @brief Extracts and lowercases the domain part of an email address.
 *
 * Writes the normalised domain into @p out (NUL-terminated).
 * Handles leading/trailing whitespace on the whole email.
 *
 * @param email   Source email string (not modified).
 * @param out     Destination buffer.
 * @param out_len Size of @p out in bytes.
 * @return 0 on success, -1 if the email is NULL, empty, has no '@', or the
 *         domain part is empty.
 * @note Neither @p email nor @p out is freed by this function.
 */
int email_domain_normalize(const char *email, char *out, size_t out_len);

/**
 * @brief Checks that a domain has at least one DNS record capable of
 *        receiving mail (MX, then A, then AAAA).
 *
 * Tries res_search(MX) first; falls back to getaddrinfo(AF_INET) then
 * getaddrinfo(AF_INET6). Blocking call - do not call from a hot loop.
 *
 * Controlled by the EMAIL_DNS_CHECK environment variable:
 *   "0" -> always returns 1 (skip, useful in dev/test).
 *   any other value (or unset) -> perform the real lookup.
 *
 * @param domain NUL-terminated lowercase domain string.
 * @return 1 if at least one record was found, 0 otherwise.
 * @note @p domain is not freed by this function.
 */
int email_dns_can_receive(const char *domain);
