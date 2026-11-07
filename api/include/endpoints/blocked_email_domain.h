#pragma once

/**
 * @file endpoints/blocked_email_domain.h
 * @brief Blocklist of disposable email domains: endpoint handlers.
 */

#include <lib/mongoose.h>
#include <structs.h>

/**
 * @brief Handles GET/POST /blocked-domain — list the blocked domains or block
 *        a new one.
 *
 * GET: returns a plain JSON array of domains. It needs an authenticated author,
 *      unless the environment variable BLOCKED_DOMAIN_LIST_PUBLIC is "1".
 * POST: body {"domain": "mailinator.com"}. The domain is cleaned (lowercase, no
 *       trailing dot) before it is stored. Requires an author (201, or 409 when
 *       the domain is already blocked).
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
 * @brief Handles DELETE /blocked-domain/:domain — unblock a domain.
 *
 * Requires an authenticated author. The domain is cleaned before the lookup, so
 * "Example.COM" removes "example.com".
 *
 * @param c           Active Mongoose connection.
 * @param msg         Parsed HTTP message.
 * @param domain      Domain from the URL (not freed).
 * @param error_reply Pre-allocated error reply structure.
 * @param secret      JWT signing secret (not freed).
 */
void send_blocked_email_domain_res(struct mg_connection *c,
                                   struct mg_http_message *msg,
                                   const char *domain,
                                   struct error_reply *error_reply,
                                   const char *secret);
