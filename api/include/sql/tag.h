#pragma once

/**
 * @file sql/tag.h
 * @brief SQLite data-access functions for the Tag table.
 */

#include <stddef.h>
#include <structs.h>
#include <lib/mongoose.h>

/**
 * @brief Checks whether a tag with the given name exists.
 * @param name Tag name (primary key).
 * @return 1 if it exists, 0 if not, negative on SQL error.
 * @note @p name is not freed by this function.
 */
int tag_exists(char *name);

/**
 * @brief Returns the total number of tags matching an optional query.
 * @param q Optional search query (empty/NULL = no filter).
 * @return Count of matching tags, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_tags_len(const struct mg_str *q);

/**
 * @brief Fetches a page of tags into a pre-allocated array.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct tag *` pointers.
 * @param q         Optional search query.
 * @param sort      Optional sort expression.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_tag(). Input pointers are not freed by this function.
 */
int get_tags(size_t len, struct tag **arr, const struct mg_str *q,
             const struct mg_str *sort, int page, int page_size);

/**
 * @brief Fetches a single tag by name.
 * @param tag  Pre-allocated structure to fill.
 * @param name Tag name to look up.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note @p name is not freed. Dynamic fields in @p tag must be freed via
 *       free_tag().
 */
int get_tag(struct tag *tag, char *name);

/**
 * @brief Inserts a new tag record.
 * @param tag Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p tag is not freed by this function.
 */
int add_tag(struct tag *tag);

/**
 * @brief Updates an existing tag record.
 * @param tag  Structure containing the updated values.
 * @param name Current name of the tag to update (may differ from tag->name
 *             when renaming).
 * @return 0 on success, http_res_code on error.
 * @note Neither @p tag nor @p name is freed by this function.
 */
int edit_tag(struct tag *tag, char *name);

/**
 * @brief Deletes a tag by name.
 * @param name Tag name (primary key).
 * @return 0 on success, http_res_code on error.
 * @note @p name is not freed by this function.
 */
int delete_tag(char *name);
