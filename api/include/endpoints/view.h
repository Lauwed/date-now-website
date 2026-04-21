#pragma once

/**
 * @file endpoints/view.h
 * @brief Page-view collection endpoint handler.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /view — list all views or record a new one.
 *
 * GET: returns a paginated JSON list filtered by optional `issueId`, `sort`,
 *      `page`, and `limit` query parameters. Requires authentication.
 * POST: validates the body, checks that the referenced issue exists, inserts
 *       the view record, and replies 201 with
 *       `{ "id": N, "time": T, "issueId": I }`. No authentication required.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret used for GET authentication (not freed).
 */
void send_views_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret);
