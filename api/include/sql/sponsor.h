#pragma once

/**
 * @file sql/sponsor.h
 * @brief SQLite data-access functions for the Sponsor table.
 */

#include <stddef.h>
#include <structs.h>
#include <lib/mongoose.h>

/**
 * @brief Checks whether a sponsor with the given name exists.
 * @param name Sponsor name (primary key).
 * @return 1 if it exists, 0 if not, negative on SQL error.
 * @note @p name is not freed by this function.
 */
int sponsor_exists(char *name);

/**
 * @brief Returns the total number of sponsors matching an optional query.
 * @param q Optional search query (empty/NULL = no filter).
 * @return Count of matching sponsors, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_sponsors_len(const struct mg_str *q);

/**
 * @brief Fetches a page of sponsors into a pre-allocated array.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct sponsor *` pointers.
 * @param q         Optional search query.
 * @param sort      Optional sort expression.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_sponsor(). Input pointers are not freed by this function.
 */
int get_sponsors(size_t len, struct sponsor **arr, const struct mg_str *q,
                 const struct mg_str *sort, int page, int page_size);

/**
 * @brief Fetches a single sponsor by name.
 * @param sponsor Pre-allocated structure to fill.
 * @param name    Sponsor name to look up.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note @p name is not freed. Dynamic fields in @p sponsor must be freed via
 *       free_sponsor().
 */
int get_sponsor(struct sponsor *sponsor, char *name);

/**
 * @brief Inserts a new sponsor record.
 * @param sponsor Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p sponsor is not freed by this function.
 */
int add_sponsor(struct sponsor *sponsor);

/**
 * @brief Updates an existing sponsor record.
 * @param sponsor Structure containing the updated values.
 * @param name    Current name of the sponsor (may differ from sponsor->name
 *                when renaming).
 * @return 0 on success, http_res_code on error.
 * @note Neither @p sponsor nor @p name is freed by this function.
 */
int edit_sponsor(struct sponsor *sponsor, char *name);

/**
 * @brief Deletes a sponsor by name.
 * @param name Sponsor name (primary key).
 * @return 0 on success, http_res_code on error.
 * @note @p name is not freed by this function.
 */
int delete_sponsor(char *name);
