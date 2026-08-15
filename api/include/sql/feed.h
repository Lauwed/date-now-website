#pragma once

/**
 * @file sql/feed.h
 * @brief SQLite data-access functions for the Feed table.
 */

#include <stddef.h>
#include <structs.h>
#include <lib/mongoose.h>

/**
 * @brief Checks whether a feed with the given id exists.
 * @param id Feed identifier.
 * @return 1 if it exists, 0 if not, negative on SQL error.
 */
int feed_exists(int id);

/**
 * @brief Returns the total number of feeds matching an optional query.
 * @param q Optional search query (empty/NULL = no filter).
 * @return Count of matching feeds, or a negative http_res_code on error.
 * @note @p q is not freed by this function.
 */
int get_feeds_len(const struct mg_str *q);

/**
 * @brief Fetches a page of feeds into a pre-allocated array.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct feed *` pointers.
 * @param q         Optional search query.
 * @param sort      Optional sort expression.
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_feed(). Input pointers are not freed by this function.
 */
int get_feeds(size_t len, struct feed **arr, const struct mg_str *q,
             const struct mg_str *sort, int page, int page_size);

/**
 * @brief Fetches a single feed by id.
 * @param feed Pre-allocated structure to fill.
 * @param id   Feed identifier to look up.
 * @return 0 on success, HTTP_NOT_FOUND if not found, HTTP_INTERNAL_ERROR on
 *         SQL error.
 * @note Dynamic fields in @p feed must be freed via free_feed().
 */
int get_feed(struct feed *feed, int id);

/**
 * @brief Inserts a new feed record.
 * @param feed Hydrated structure to insert.
 * @return 0 on success, http_res_code on error.
 * @note @p feed is not freed by this function. @c feed->id is set to the
 *       newly inserted row id on success.
 */
int add_feed(struct feed *feed);

/**
 * @brief Updates an existing feed record.
 * @param feed Structure containing the updated values (matched by @c feed->id).
 * @return 0 on success, http_res_code on error.
 * @note @p feed is not freed by this function.
 */
int edit_feed(struct feed *feed);

/**
 * @brief Deletes a feed by id.
 * @param id Feed identifier.
 * @return 0 on success, http_res_code on error.
 */
int delete_feed(int id);
