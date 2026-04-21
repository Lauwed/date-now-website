#pragma once

/**
 * @file endpoints/issue.h
 * @brief Issue collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /issue — list all issues or create a new one.
 *
 * GET: returns a paginated JSON list filtered by optional `q`, `status`,
 *      `sort`, `page`, and `limit` query parameters. Requires authentication
 *      for non-PUBLISHED issues.
 * POST: validates the body, inserts the issue, and returns the created object
 *       (201). Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issues_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /issue/:id — fetch, update, or delete an issue.
 *
 * GET: returns the full issue object with its tags, authors, and sponsors.
 * PUT: updates the issue and returns the updated object. Requires authentication.
 * DELETE: deletes the issue. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param id          Issue database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply,
                    const char *secret);

/**
 * @brief Handles POST /issue/:id/publish — publishes a draft issue.
 *
 * Sets @c published_at to the current timestamp. Requires authentication.
 * Replies 200 with `{ "message": "Issue successfully published" }`.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param id          Issue database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void publish_issue_res(struct mg_connection *c, struct mg_http_message *msg,
                       int id, struct error_reply *error_reply,
                       const char *secret);
