#pragma once

/**
 * @file endpoints/category.h
 * @brief Category collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /category — list all categories or create a new one.
 *
 * GET: returns the full category list as a plain JSON array (no pagination
 *      wrapper). Public, no authentication required.
 * POST: validates the body, inserts the category, and returns the created
 *       category object (201). Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_categories_res(struct mg_connection *c, struct mg_http_message *msg,
                         struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /category/:name — fetch, update, or delete
 *        a category.
 *
 * GET: returns the category object. Public, no authentication required.
 * PUT: updates the category and returns the updated object. Requires
 *      authentication.
 * DELETE: deletes the category (cascades to IssueSection). Requires
 *         authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param name        Category name (primary key). Not freed by this function.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_category_res(struct mg_connection *c, struct mg_http_message *msg,
                       char *name, struct error_reply *error_reply,
                       const char *secret);
