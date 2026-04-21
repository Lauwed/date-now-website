#pragma once

/**
 * @file endpoints/sponsor.h
 * @brief Sponsor collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /sponsor — list all sponsors or create a new one.
 *
 * GET: returns the full sponsor list as a plain JSON array (no pagination
 *      wrapper).
 * POST: validates the body, inserts the sponsor, and returns the created
 *       sponsor object (201). Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_sponsors_res(struct mg_connection *c, struct mg_http_message *msg,
                       struct error_reply *error_reply, const char *secret);

/**
 * @brief Handles GET/PUT/DELETE /sponsor/:name — fetch, update, or delete a
 *        sponsor.
 *
 * GET: returns the sponsor object.
 * PUT: updates the sponsor and returns the updated object. Requires
 *      authentication.
 * DELETE: deletes the sponsor. Requires authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param name        Sponsor name (primary key). Not freed by this function.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_sponsor_res(struct mg_connection *c, struct mg_http_message *msg,
                      char *name, struct error_reply *error_reply,
                      const char *secret);
