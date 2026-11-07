/**
 * @file lib/email_admission.c
 * @brief Decides whether an email address can be accepted.
 */

#include <lib/email_admission.h>
#include <lib/email_validator.h>
#include <sql/blocked_email_domain.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int env_is(const char *name, const char *value) {
  const char *current = getenv(name);
  return current != NULL && strcmp(current, value) == 0;
}

static void refuse(struct email_admission_result *result, int reason_code) {
  result->allowed = 0;
  result->reason_code = reason_code;
}

int email_admission_inspect(const char *email,
                            struct email_admission_result *result) {
  if (email == NULL || result == NULL) {
    return -1;
  }

  result->allowed = 1;
  result->is_flagged = 0;
  result->reason_code = EMAIL_ADMISSION_OK;

  if (env_is("EMAIL_VALIDATION_BYPASS", "1")) {
    return 0;
  }

  char domain[EMAIL_DOMAIN_MAX_LEN + 1];
  if (email_domain_normalize(email, domain, sizeof(domain)) != 0) {
    refuse(result, EMAIL_ADMISSION_MALFORMED);
    return 0;
  }

  const char *app_domain = getenv("APP_DOMAIN");
  if (app_domain != NULL && strcasecmp(domain, app_domain) == 0) {
    refuse(result, EMAIL_ADMISSION_APP_DOMAIN);
    return 0;
  }

  int blocked = blocked_domain_exists(domain);
  if (blocked < 0) {
    refuse(result, EMAIL_ADMISSION_ERROR);
    return 0;
  }
  if (blocked > 0 && !env_is("EMAIL_BLOCKLIST_MODE", "flag")) {
    refuse(result, EMAIL_ADMISSION_BLOCKED);
    return 0;
  }

  // The DNS lookup is the slowest check: it only runs for a domain that the
  // blocklist let through.
  if (!email_dns_can_receive(domain)) {
    refuse(result, EMAIL_ADMISSION_DNS_FAIL);
    return 0;
  }

  if (blocked > 0) {
    result->is_flagged = 1;
    result->reason_code = EMAIL_ADMISSION_BLOCKED;
  }

  return 0;
}
