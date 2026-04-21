#pragma once

/**
 * @file utils.h
 * @brief General-purpose utility macros.
 */

/**
 * @brief printf format specifier for a non-NUL-terminated string with an
 *        explicit length (maps to `%.*s`).
 *
 * Usage: `printf(STR_FMT, (int)len, ptr)`
 */
#define STR_FMT "%.*s"
