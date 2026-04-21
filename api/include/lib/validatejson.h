#pragma once

/**
 * @file validatejson.h
 * @brief Recursive-descent JSON validator (RFC 8259).
 *
 * Each `validate*` function advances the string pointer past the element it
 * recognises and returns `true` on success or `false` on parse error.
 * The public entry point for endpoint use is mg_validateJSON().
 */

#include <lib/mongoose.h>
#include <stdbool.h>

/** @brief Validates a JSON array starting at @p *s. Advances @p *s. */
bool validateArray(const char **s);

/** @brief Validates a JSON boolean (`true` or `false`). Advances @p *s. */
bool validateBoolean(const char **s);

/** @brief Validates the closing bracket of a JSON array (`]`). Advances @p *s. */
bool validateEndOfArray(const char **s);

/** @brief Validates the closing brace of a JSON object (`}`). Advances @p *s. */
bool validateEndOfObject(const char **s);

/** @brief Validates the optional exponent part of a JSON number. Advances @p *s. */
bool validateExponent(const char **s);

/** @brief Validates the optional fractional part of a JSON number. Advances @p *s. */
bool validateFraction(const char **s);

/** @brief Validates a complete JSON text (object or array at the root). Advances @p *s. */
bool validateJSON(const char **s);

/** @brief Validates a single JSON element (value). Advances @p *s. */
bool validateJSONElement(const char **s);

/** @brief Validates a JSON string (including surrounding quotes). Advances @p *s. */
bool validateJSONString(const char **s);

/** @brief Validates a JSON number. Advances @p *s. */
bool validateNumber(const char **s);

/** @brief Validates a JSON object. Advances @p *s. */
bool validateObject(const char **s);

/** @brief Validates an unquoted JSON string value. Advances @p *s. */
bool validateString(const char **s);

/**
 * @brief Validates a Mongoose mg_str as a JSON text.
 * @param json mg_str pointing to the raw JSON bytes.
 * @return true if @p json is valid JSON, false otherwise.
 * @note @p json is not freed or modified by this function.
 */
bool mg_validateJSON(struct mg_str json);
