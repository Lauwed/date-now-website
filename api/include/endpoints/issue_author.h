#pragma once

/**
 * @file endpoints/issue_author.h
 * @brief IssueAuthor collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /issue/:issueId/author — list or add authors.
 *
 * GET: returns a paginated JSON list of authors associated with the issue.
 *      Requires authentication.
 * POST: validates the body, checks for duplicate associations, inserts the
 *       record, and replies 201. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_authors_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id,
                            struct error_reply *error_reply,
                            const char *secret);

/**
 * @brief Handles DELETE /issue/:issueId/author/:id — removes an author.
 *
 * Deletes the IssueAuthor association. Requires authentication.
 * Replies 200 with `{ "message": "Author successfully deleted" }`.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param id          User database identifier (author to remove).
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_author_res(struct mg_connection *c, struct mg_http_message *msg,
                           int issue_id, int id,
                           struct error_reply *error_reply, const char *secret);
