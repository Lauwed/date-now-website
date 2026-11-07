/**
 * @file lib/email_validator.c
 * @brief Email domain normalisation and DNS reachability checks.
 */

#include <arpa/nameser.h>
#include <ctype.h>
#include <lib/email_validator.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DNS_BUF_LEN 1024
#define DNS_NAME_LEN 256
#define LABEL_MAX_LEN 63

/** Returns 1 if a label is 1 to 63 characters of [a-z0-9-], not starting or
 *  ending with a hyphen. */
static int label_is_valid(const char *label, size_t len) {
  if (len == 0 || len > LABEL_MAX_LEN || label[0] == '-' ||
      label[len - 1] == '-') {
    return 0;
  }

  for (size_t i = 0; i < len; i++) {
    unsigned char ch = (unsigned char)label[i];
    if (!isalnum(ch) && ch != '-') {
      return 0;
    }
  }

  return 1;
}

/** Returns 1 if a lowercase domain is made of at least two valid labels. */
static int domain_is_valid(const char *domain, size_t len) {
  if (len == 0 || len > EMAIL_DOMAIN_MAX_LEN) {
    return 0;
  }

  size_t label_start = 0;
  int labels = 0;

  for (size_t i = 0; i <= len; i++) {
    if (i < len && domain[i] != '.') {
      continue;
    }
    if (!label_is_valid(domain + label_start, i - label_start)) {
      return 0;
    }
    labels++;
    label_start = i + 1;
  }

  return labels >= 2;
}

int email_domain_clean(const char *domain, char *out, size_t out_len) {
  if (domain == NULL || out == NULL || out_len == 0) {
    return -1;
  }

  const char *end = domain + strlen(domain);
  while (end > domain && isspace((unsigned char)end[-1])) {
    end--;
  }
  while (domain < end && isspace((unsigned char)*domain)) {
    domain++;
  }

  size_t len = (size_t)(end - domain);
  if (len > 0 && domain[len - 1] == '.') {
    len--;
  }

  if (len >= out_len || len > EMAIL_DOMAIN_MAX_LEN) {
    return -1;
  }

  for (size_t i = 0; i < len; i++) {
    out[i] = (char)tolower((unsigned char)domain[i]);
  }
  out[len] = '\0';

  return domain_is_valid(out, len) ? 0 : -1;
}

int email_domain_normalize(const char *email, char *out, size_t out_len) {
  if (email == NULL) {
    return -1;
  }

  const char *at = strrchr(email, '@');
  if (at == NULL) {
    return -1;
  }

  return email_domain_clean(at + 1, out, out_len);
}

/**
 * Looks for the MX records of a domain.
 * @return 1 if one MX names a host, 0 if every MX is a "null MX" (the domain
 *         does not receive mail), -1 if there is no MX record.
 */
static int mx_answer(const char *domain) {
  unsigned char buf[DNS_BUF_LEN];
  int len = res_search(domain, C_IN, T_MX, buf, sizeof(buf));
  if (len <= 0) {
    return -1;
  }

  ns_msg msg;
  if (len > (int)sizeof(buf) || ns_initparse(buf, len, &msg) < 0) {
    return -1;
  }

  int found = 0;
  for (int i = 0; i < ns_msg_count(msg, ns_s_an); i++) {
    ns_rr rr;
    char host[DNS_NAME_LEN];
    if (ns_parserr(&msg, ns_s_an, i, &rr) < 0 || ns_rr_type(rr) != T_MX) {
      continue;
    }
    // An MX is the 16-bit preference, then the name of the host.
    if (dn_expand(ns_msg_base(msg), ns_msg_end(msg), ns_rr_rdata(rr) + 2, host,
                  sizeof(host)) < 0) {
      continue;
    }
    if (host[0] != '\0') {
      return 1;
    }
    found = 1;
  }

  return found ? 0 : -1;
}

/** Returns 1 if the domain has a record of the given type (T_A or T_AAAA). */
static int has_address(const char *domain, int type) {
  unsigned char buf[DNS_BUF_LEN];
  return res_search(domain, C_IN, type, buf, sizeof(buf)) > 0;
}

int email_dns_can_receive(const char *domain) {
  if (domain == NULL || *domain == '\0') {
    return 0;
  }

  // Respect EMAIL_DNS_CHECK=0 to skip lookups in dev/test
  const char *dns_check = getenv("EMAIL_DNS_CHECK");
  if (dns_check != NULL && dns_check[0] == '0') {
    return 1;
  }

  int mx = mx_answer(domain);
  if (mx >= 0) {
    return mx;
  }

  return has_address(domain, T_A) || has_address(domain, T_AAAA);
}
