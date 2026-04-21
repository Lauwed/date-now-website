#pragma once

/**
 * @file sql/user.h
 * @brief SQLite data-access functions for the User table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Checks whether a user with the given id exists.
 * @param id Database identifier.
 * @return 1 if the user exists, 0 if not, negative on SQL error.
 */
int user_exists(int id);

/**
 * @brief Checks whether a user with the given username or email already exists.
 * @param username Username to check (may be NULL to skip).
 * @param email    Email to check (may be NULL to skip).
 * @return 1 if a conflicting record exists, 0 if not, negative on SQL error.
 * @note Neither @p username nor @p email is freed by this function.
 */
int user_identity_exists(char *username, char *email);

/**
 * @brief Returns the total number of users matching an optional search query.
 * @param q Optional search query as a mg_str (empty/NULL = no filter).
 * @return Count of matching users, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_users_len(const struct mg_str *q);

/**
 * @brief Fetches a page of users into a pre-allocated array.
 * @param len       Size of @p arr (number of elements to fill).
 * @param arr       Pre-allocated array of `struct user *` pointers.
 * @param q         Optional search query (empty/NULL = no filter).
 * @param sort      Optional sort expression (empty/NULL = default order).
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Number of records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element of @p arr is dynamically allocated and must be freed
 *       via free_user(). @p q and @p sort are not freed by this function.
 */
int get_users(size_t len, struct user **arr, const struct mg_str *q,
              const struct mg_str *sort, int page, int page_size);

/**
 * @brief Fetches a single user by id.
 * @param user Pre-allocated and initialised structure to fill.
 * @param id   Database identifier.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note Dynamic fields in @p user are allocated by user_map() and must be
 *       freed via free_user().
 */
int get_user(struct user *user, int id);

/**
 * @brief Fetches a single user by email address.
 * @param user  Pre-allocated structure to fill.
 * @param email Email address to look up.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note @p email is not freed by this function. Dynamic fields in @p user
 *       must be freed via free_user().
 */
int get_user_by_email(struct user *user, char *email);

/**
 * @brief Fetches the TOTP seed for a user identified by email.
 * @param email Email address of the user.
 * @param seed  Output pointer; set to a newly allocated NUL-terminated string
 *              containing the base32 seed. Caller must free.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note @p email is not freed. *@p seed is allocated by this function and
 *       must be freed by the caller.
 */
int get_user_totp_seed(char *email, char **seed);

/**
 * @brief Inserts a new user record.
 * @param user Hydrated structure to insert. @c user->id is set to the
 *             last-insert rowid on success.
 * @return 0 on success, http_res_code on error.
 * @note @p user is not freed by this function.
 */
int add_user(struct user *user);

/**
 * @brief Updates an existing user record.
 * @param user Structure containing the updated values and the target @c id.
 * @return 0 on success, http_res_code on error.
 * @note @p user is not freed by this function.
 */
int edit_user(struct user *user);

/**
 * @brief Deletes a user by id.
 * @param id Database identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_user(int id);

/**
 * @brief Fetches the email addresses of all newsletter subscribers.
 * @param len    Output: number of addresses returned.
 * @param emails Output: dynamically allocated array of NUL-terminated strings.
 *               The array and each string must be freed by the caller.
 * @return 0 on success, http_res_code on error.
 */
int get_subscriber_emails(size_t *len, char ***emails);
