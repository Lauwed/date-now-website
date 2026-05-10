#pragma once

/**
 * @file endpoints/blocked_email_domain.h
 * @brief Blocked email domain collection and single-resource endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /blocked-domain — list domains or add a new one.
 *
 * GET: returns the list of blocked domains. Visibility is controlled by
 *      the BLOCKED_DOMAIN_LIST_PUBLIC env var ("1" = public, otherwise
 *      AUTHOR authentication is required).
 * POST: adds a new domain to the blocklist. Requires AUTHOR authentication.
 *       Body: {"domain": "example.com"}
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_blocked_email_domains_res(struct mg_connection *c,
                                    struct mg_http_message *msg,
                                    struct error_reply *error_reply,
                                    const char *secret);

/**
 * @brief Handles DELETE /blocked-domain/:domain — remove a blocked domain.
 *
 * Requires AUTHOR authentication.
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param domain      URL-decoded domain string (not freed by this function).
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_blocked_email_domain_res(struct mg_connection *c,
                                   struct mg_http_message *msg,
                                   const char *domain,
                                   struct error_reply *error_reply,
                                   const char *secret);
