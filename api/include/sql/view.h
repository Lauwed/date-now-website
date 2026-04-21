#pragma once

/**
 * @file sql/view.h
 * @brief SQLite data-access functions for the View (page-view) table.
 */

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

/**
 * @brief Returns the total number of views, optionally filtered by issue.
 * @param issue_id Issue id to filter on (0 = return all views).
 * @return Count of matching views, or a negative http_res_code on error.
 */
int get_views_len(int issue_id);

/**
 * @brief Fetches a page of views into a pre-allocated array.
 * @param len       Size of @p arr.
 * @param arr       Pre-allocated array of `struct view *` pointers.
 * @param issue_id  Issue id filter (0 = no filter).
 * @param sort      Optional sort expression (empty/NULL = default order).
 * @param page      Page number (1-based; -1 = no pagination).
 * @param page_size Records per page.
 * @return 0 on success, http_res_code on error.
 * @note Each element in @p arr is dynamically allocated and must be freed
 *       via free_view(). @p sort is not freed by this function.
 */
int get_views(size_t len, struct view **arr, int issue_id,
              const struct mg_str *sort, int page, int page_size);

/**
 * @brief Inserts a new view record.
 * @param view Hydrated structure to insert. @c view->id is set to the
 *             last-insert rowid on success.
 * @return 0 on success, http_res_code on error.
 * @note @p view is not freed by this function.
 */
int add_view(struct view *view);
