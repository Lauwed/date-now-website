#pragma once

/**
 * @file endpoints/user.h
 * @brief User collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /user — list all users or create a new one.
 *
 * GET: returns a paginated JSON list filtered by optional `q`, `sort`,
 *      `page`, and `limit` query parameters. Requires authentication.
 * POST: validates the body, inserts the user, and returns the created user
 *       object (201). Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_users_res(struct mg_connection *c, struct mg_http_message *msg,
                    struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /user/:id — fetch, update, or delete a user.
 *
 * GET: returns the full user object. Requires authentication.
 * PUT: updates the user and returns the updated object. Requires authentication.
 * DELETE: deletes the user. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param id          User database identifier.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_user_res(struct mg_connection *c, struct mg_http_message *msg, int id,
                   struct error_reply *error_reply, const char *secret);
