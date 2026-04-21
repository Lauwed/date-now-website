#pragma once

/**
 * @file endpoints/issue_sponsor.h
 * @brief IssueSponsor collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /issue/:issueId/sponsor — list or add sponsors.
 *
 * GET: returns a paginated JSON list of issue_sponsor records for the issue.
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
void send_issue_sponsors_res(struct mg_connection *c,
                             struct mg_http_message *msg, int issue_id,
                             struct error_reply *error_reply,
                             const char *secret);

/**
 * @brief Handles DELETE /issue/:issueId/sponsor/:name — removes a sponsor
 *        association.
 *
 * Deletes the IssueSponsor association. Requires authentication.
 * Replies 200 with `{ "message": "Sponsor successfully deleted" }`.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param issue_id    Issue database identifier.
 * @param name        Sponsor name. Not freed by this function.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_issue_sponsor_res(struct mg_connection *c,
                            struct mg_http_message *msg, int issue_id,
                            char *name, struct error_reply *error_reply,
                            const char *secret);
