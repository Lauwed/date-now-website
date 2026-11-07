#pragma once

/**
 * @file lib/email_admission.h
 * @brief Decides whether an email address can be accepted: its domain, the
 *        service's own domain, the blocklist and DNS.
 */

/**
 * @brief Result of an email admission inspection.
 *
 * The @c reason_code field uses the EMAIL_ADMISSION_* constants of
 * lib/email_validator.h. No dynamic allocation: safe to use on the stack.
 */
struct email_admission_result {
  int allowed;     /**< 1 = the email can be accepted. */
  int is_flagged;  /**< 1 = accepted, but the user must be flagged in DB. */
  int reason_code; /**< EMAIL_ADMISSION_* constant, 0 when no issue. */
};

/**
 * @brief Inspects an email address.
 *
 * The checks run from the cheapest to the slowest, and stop at the first one
 * that refuses the email:
 *  1. EMAIL_VALIDATION_BYPASS=1 accepts everything.
 *  2. The domain is read and checked (email_domain_normalize()).
 *  3. APP_DOMAIN (the service's own domain) is refused.
 *  4. The BlockedEmailDomain table: with EMAIL_BLOCKLIST_MODE=flag the email
 *     is accepted and flagged, otherwise (default "reject") it is refused.
 *  5. DNS (email_dns_can_receive()): a domain that cannot receive mail is
 *     refused.
 *
 * @param email  Source email string (not modified).
 * @param result Output: filled admission result. Must not be NULL.
 * @return 0 on success, -1 if @p email or @p result is NULL.
 * @note A blocklist that cannot be read gives @c allowed = 0 with
 *       EMAIL_ADMISSION_ERROR: the endpoint answers with an internal error.
 *       Neither @p email nor @p result is freed by this function.
 */
int email_admission_inspect(const char *email,
                            struct email_admission_result *result);
