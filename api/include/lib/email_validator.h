#pragma once

/**
 * @file lib/email_validator.h
 * @brief Email domain normalisation and DNS reachability checks.
 *
 * These functions are pure utilities with no database dependency. The
 * admission orchestration (blocklist, own domain, DNS) lives in utils.c, in
 * email_admission_inspect().
 */

#include <stddef.h>

/* Reason codes stored in email_admission_result.reason_code. */
#define EMAIL_ADMISSION_OK 0         /**< Accepted, no issue. */
#define EMAIL_ADMISSION_DNS_FAIL 1   /**< Domain has no DNS record for mail. */
#define EMAIL_ADMISSION_BLOCKED 2    /**< Domain is in BlockedEmailDomain. */
#define EMAIL_ADMISSION_APP_DOMAIN 3 /**< Domain matches APP_DOMAIN. */
#define EMAIL_ADMISSION_MALFORMED 4  /**< The domain part is not valid. */
#define EMAIL_ADMISSION_ERROR 5      /**< The blocklist could not be read. */

/** @brief Longest domain name, without the trailing dot (RFC 1035). */
#define EMAIL_DOMAIN_MAX_LEN 253

/**
 * @brief Validates and lowercases a domain name.
 *
 * The spaces around the domain and one trailing dot are removed. The domain
 * must be ASCII (an internationalised domain is written in punycode), at most
 * 253 characters long, and made of at least two labels of 1 to 63 letters,
 * digits or hyphens, none of which starts or ends with a hyphen.
 *
 * @param domain  Source domain string (not modified).
 * @param out     Destination buffer, NUL-terminated on success.
 * @param out_len Size of @p out in bytes (EMAIL_DOMAIN_MAX_LEN + 1 is enough).
 * @return 0 on success, -1 if the domain is NULL or is not valid.
 * @note Neither @p domain nor @p out is freed by this function.
 */
int email_domain_clean(const char *domain, char *out, size_t out_len);

/**
 * @brief Extracts the domain of an email, the part after the last '@', and
 *        cleans it with email_domain_clean().
 *
 * @param email   Source email string (not modified).
 * @param out     Destination buffer, NUL-terminated on success.
 * @param out_len Size of @p out in bytes (EMAIL_DOMAIN_MAX_LEN + 1 is enough).
 * @return 0 on success, -1 if the email is NULL, has no '@' or has a domain
 *         that is not valid.
 * @note Neither @p email nor @p out is freed by this function.
 */
int email_domain_normalize(const char *email, char *out, size_t out_len);

/**
 * @brief Checks that a domain can receive mail: an MX record, or when there is
 *        none an A or AAAA record (RFC 5321, section 5.1).
 *
 * A "null MX" (RFC 7505: a single MX whose target is ".") says that the domain
 * does not receive mail, so the domain is refused without looking at A/AAAA.
 * The lookups block until the resolver answers or gives up, so the time they
 * take is set by the resolver configuration.
 *
 * Controlled by the EMAIL_DNS_CHECK environment variable:
 *   "0" -> always returns 1 (skip, useful in dev/test).
 *   any other value (or unset) -> perform the real lookup.
 *
 * @param domain NUL-terminated lowercase domain, from email_domain_normalize().
 * @return 1 if the domain can receive mail, 0 otherwise.
 * @note @p domain is not freed by this function.
 */
int email_dns_can_receive(const char *domain);
