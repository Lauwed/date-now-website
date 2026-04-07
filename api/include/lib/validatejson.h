#include <lib/mongoose.h>
#include <stdbool.h>

bool validateArray(const char **);
bool validateBoolean(const char **);
bool validateEndOfArray(const char **);
bool validateEndOfObject(const char **);
bool validateExponent(const char **);
bool validateFraction(const char **);
bool validateJSON(const char **);
bool validateJSONElement(const char **);
bool validateJSONString(const char **);
bool validateNumber(const char **);
bool validateObject(const char **);
bool validateString(const char **);

bool mg_validateJSON(struct mg_str json);
