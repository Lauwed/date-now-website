#pragma once

#include <stdint.h>

uint32_t totp_generate(const char *base32_secret, uint32_t step_seconds);
int totp_generate_secret(char *output);
