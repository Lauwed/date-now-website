#include <lib/mongoose.h>
#include <lib/validatejson.h>
#include <macros/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool validateCharAndAdvanceCursor(const char **jsonString, char c) {
  return **jsonString == c && ++(*jsonString);
}

bool skipWhitespace(const char **jsonString) {
  while (**jsonString != '\0' &&
         (validateCharAndAdvanceCursor(jsonString, ' ') ||
          validateCharAndAdvanceCursor(jsonString, '\t') ||
          validateCharAndAdvanceCursor(jsonString, '\r') ||
          validateCharAndAdvanceCursor(jsonString, '\n')))
    ;
  return true;
}

bool validateEndOfObject(const char **jsonString) {
  (*jsonString)++;
  skipWhitespace(jsonString);
  return **jsonString == '}' ||
         validateCharAndAdvanceCursor(jsonString, ',') &&
             skipWhitespace(jsonString) && **jsonString == '"' &&
             validateString(jsonString) && (*jsonString)++ &&
             skipWhitespace(jsonString) &&
             validateCharAndAdvanceCursor(jsonString, ':') &&
             validateJSONElement(jsonString) && validateEndOfObject(jsonString);
}

bool validateObject(const char **jsonString) {
  (*jsonString)++;
  skipWhitespace(jsonString);
  return **jsonString == '}' ||
         **jsonString == '"' && validateString(jsonString) && (*jsonString)++ &&
             skipWhitespace(jsonString) &&
             validateCharAndAdvanceCursor(jsonString, ':') &&
             validateJSONElement(jsonString) && validateEndOfObject(jsonString);
}

bool validateEndOfArray(const char **jsonString) {
  (*jsonString)++;
  skipWhitespace(jsonString);
  return **jsonString == ']' || validateCharAndAdvanceCursor(jsonString, ',') &&
                                    validateJSONElement(jsonString) &&
                                    validateEndOfArray(jsonString);
}

bool validateArray(const char **jsonString) {
  (*jsonString)++;
  skipWhitespace(jsonString);
  return **jsonString == ']' ||
         validateJSONElement(jsonString) && validateEndOfArray(jsonString);
}

bool validateBoolean(const char **jsonString) {
  return (strncmp(*jsonString, "true", 4) == 0 ||
          strncmp(*jsonString, "null", 4) == 0) &&
             (*jsonString = *jsonString + 3) ||
         strncmp(*jsonString, "false", 5) == 0 &&
             (*jsonString = *jsonString + 4);
}

bool validateString(const char **jsonString) {
  (*jsonString)++;
  while (**jsonString != '\0' && **jsonString != '"') {
    if (**jsonString == '\\') {
      (*jsonString)++;
      if (**jsonString == 'u') {
        // From
        // https://github.com/zserge/jsmn/blob/25647e692c7906b96ffd2b05ca54c097948e879c/jsmn.h#L241-L251
        for (int x = 0; x < 4; (*jsonString)++ && x++) {
          int c = *(*jsonString + 1);
          if (!((c >= 48 && c <= 57) || /* 0-9 */
                (c >= 65 && c <= 70) || /* A-F */
                (c >= 97 && c <= 102)   /* a-f */
                ))
            return false;
        }
      }
    }
    (*jsonString)++;
  }
  return **jsonString == '"';
}

bool validateAtLeastOneInteger(const char **jsonString) {
  if (**jsonString < 48 || **jsonString > 57)
    return false;
  do
    (*jsonString)++;
  while (**jsonString != '\0' && **jsonString >= 48 && **jsonString <= 57);
  return true;
}

bool validateExponent(const char **jsonString) {
  return (**jsonString != 'e' && **jsonString != 'E') ||
         (*jsonString)++ &&
             (validateCharAndAdvanceCursor(jsonString, '-') ||
              validateCharAndAdvanceCursor(jsonString, '+') || true) &&
             validateAtLeastOneInteger(jsonString);
}

bool validateFraction(const char **jsonString) {
  return **jsonString != '.' ||
         (*jsonString)++ && validateAtLeastOneInteger(jsonString);
}

bool validateNumber(const char **jsonString) {
  return validateAtLeastOneInteger(jsonString) &&
         validateFraction(jsonString) && validateExponent(jsonString) &&
         (**jsonString == '}' || **jsonString == ']' || **jsonString == ',' ||
          **jsonString == ' ' || **jsonString == '\t' || **jsonString == '\r' ||
          **jsonString == '\n' || **jsonString == '\0') &&
         (*jsonString)--;
}

bool validateJSONElement(const char **jsonString) {
  skipWhitespace(jsonString);
  switch (**jsonString) {
  case '"':
    return validateString(jsonString);
  case '[':
    return validateArray(jsonString);
  case '{':
    return validateObject(jsonString);
  case 't':
  case 'f':
  case 'n':
    return validateBoolean(jsonString);
  case '-':
    (*jsonString)++;
  default:
    return validateNumber(jsonString);
  }
}

bool validateJSONString(const char **jsonString) {
  return validateJSONElement(jsonString) &&
         (**jsonString == '\0' || ++(*jsonString) &&
                                      skipWhitespace(jsonString) &&
                                      **jsonString == '\0');
}

bool validateJSON(const char **jsonString) {
  return validateJSONString(jsonString);
}

bool mg_validateJSON(struct mg_str json) {
  char *str = malloc(json.len + 1);
  sprintf(str, STR_FMT, (int)json.len, json.buf);

  bool result = validateJSON((const char **)&str);

  size_t offset = json.len - strlen(str);
  free(str - offset);

  return result;
}
