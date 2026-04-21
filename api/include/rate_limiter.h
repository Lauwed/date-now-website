#pragma once

#include <lib/mongoose.h>

#define RATE_LIMIT_AUTH_MAX     20
#define RATE_LIMIT_AUTH_WINDOW  60
#define RATE_LIMIT_TOTP_MAX      5
#define RATE_LIMIT_TOTP_WINDOW  60

/**
 * Returns 0 if the request is within the allowed rate, 1 if it should be
 * rejected. Uses a fixed window counter keyed on the remote IP address.
 */
int rate_limit_check(struct mg_addr *addr, int max_requests, int window_seconds);
