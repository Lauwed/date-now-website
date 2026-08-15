#pragma once

/**
 * @file sql/category.h
 * @brief SQLite data-access functions for the Category table.
 */

#include <stddef.h>
#include <structs.h>
#include <lib/mongoose.h>

/**
 * @brief Checks whether a category with the given name exists.
 * @param name Category name (primary key).
 * @return 1 if it exists, 0 if not, negative on SQL error.
 * @note @p name is not freed by this function.
 */
int category_exists(char *name);

/**
 * @brief Returns the total number of categories matching an optional query.
 * @param q Optional search query (empty/NULL = no filter).
 * @return Count of matching categories, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_categories_len(const struct mg_str *q);

/**
 * @brief Fetches a page of categories into a pre-allocated array.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct category *` pointers.
 * @param q         Optional search query.
 * @param sort      Optional sort expression.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_category(). Input pointers are not freed by this function.
 */
int get_categories(size_t len, struct category **arr, const struct mg_str *q,
                   const struct mg_str *sort, int page, int page_size);

/**
 * @brief Fetches a single category by name.
 * @param category Pre-allocated structure to fill.
 * @param name     Category name to look up.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note @p name is not freed. Dynamic fields in @p category must be freed
 *       via free_category().
 */
int get_category(struct category *category, char *name);

/**
 * @brief Inserts a new category record.
 * @param category Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p category is not freed by this function.
 */
int add_category(struct category *category);

/**
 * @brief Updates an existing category record.
 * @param category Structure containing the updated values.
 * @param name     Current name of the category to update (may differ from
 *                 category->name when renaming).
 * @return 0 on success, http_res_code on error.
 * @note Neither @p category nor @p name is freed by this function.
 */
int edit_category(struct category *category, char *name);

/**
 * @brief Deletes a category by name.
 * @param name Category name (primary key).
 * @return 0 on success, http_res_code on error.
 * @note @p name is not freed by this function.
 */
int delete_category(char *name);
