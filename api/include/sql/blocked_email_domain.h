#pragma once

/**
 * @file sql/blocked_email_domain.h
 * @brief Postgres data-access functions for the BlockedEmailDomain table.
 */

#include <stddef.h>

/**
 * @brief Checks whether a domain is in the blocklist.
 * @param domain Lowercase domain, without a trailing dot.
 * @return 1 if it is blocked, 0 if not, negative on SQL error.
 * @note @p domain is not freed by this function.
 */
int blocked_domain_exists(const char *domain);

/**
 * @brief Fetches every blocked domain, in alphabetical order.
 * @param arr Output: array of dynamically allocated strings (NULL when empty).
 * @param len Output: number of elements in @p arr.
 * @return 0 on success, http_res_code on error.
 * @note Free the result with free_blocked_domains().
 */
int get_blocked_domains(char ***arr, size_t *len);

/**
 * @brief Frees the array filled by get_blocked_domains().
 * @param arr Array of strings (may be NULL).
 * @param len Number of elements in @p arr.
 */
void free_blocked_domains(char **arr, size_t len);

/**
 * @brief Adds a domain to the blocklist.
 * @param domain Lowercase domain, without a trailing dot.
 * @return 0 on success (also if it was already blocked), http_res_code on
 *         error.
 * @note @p domain is not freed by this function.
 */
int add_blocked_domain(const char *domain);

/**
 * @brief Removes a domain from the blocklist.
 * @param domain Lowercase domain, without a trailing dot.
 * @return 0 on success, HTTP_NOT_FOUND if it was not blocked,
 *         HTTP_INTERNAL_ERROR on SQL error.
 * @note @p domain is not freed by this function.
 */
int delete_blocked_domain(const char *domain);
