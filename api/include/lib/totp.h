#pragma once

#include <stdint.h>
#include <time.h>

uint32_t totp_generate(const char *base32_secret, uint32_t step_seconds);
uint32_t totp_generate_at(const char *base32_secret, uint32_t step_seconds,
                          time_t timestamp);
int totp_generate_secret(char *output);
