#pragma once

/**
 * @file sql/blocked_email_domain.h
 * @brief SQLite data-access functions for the BlockedEmailDomain table.
 */

#include <stddef.h>

/**
 * @brief Checks whether a domain is in the blocklist.
 * @param domain NUL-terminated domain string (not freed by this function).
 * @return 1 if blocked, 0 if not, negative on SQL error.
 */
int blocked_domain_exists(const char *domain);

/**
 * @brief Returns the total number of blocked domains.
 * @return Count, or negative on SQL error.
 */
int get_blocked_domains_len(void);

/**
 * @brief Fetches all blocked domains into a newly allocated array of strings.
 * @param arr Output: dynamically allocated array of NUL-terminated strings.
 *            Both the array and each string must be freed by the caller.
 * @param len Output: number of entries written.
 * @return 0 on success, http_res_code on error.
 */
int get_blocked_domains(char ***arr, size_t *len);

/**
 * @brief Inserts a new blocked domain.
 * @param domain Domain to add (not freed by this function).
 * @return 0 on success, SQLITE_CONSTRAINT if already exists, other SQLITE_*
 *         code on error.
 */
int add_blocked_domain(const char *domain);

/**
 * @brief Deletes a blocked domain.
 * @param domain Domain to remove (not freed by this function).
 * @return 0 on success, HTTP_NOT_FOUND if absent, other SQLITE_* on error.
 */
int delete_blocked_domain(const char *domain);
