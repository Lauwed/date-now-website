/**
 * @file lib/email_validator.c
 * @brief Email domain normalisation and DNS reachability checks.
 */

#include <arpa/nameser.h>
#include <ctype.h>
#include <lib/email_validator.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define DNS_BUF_LEN 512

int email_domain_normalize(const char *email, char *out, size_t out_len) {
  if (email == NULL || out == NULL || out_len == 0) {
    return -1;
  }

  // Trim leading whitespace
  while (*email == ' ' || *email == '\t' || *email == '\r' || *email == '\n')
    email++;

  // Find '@'
  const char *at = strchr(email, '@');
  if (at == NULL) {
    return -1;
  }

  // Domain starts after '@'
  const char *domain = at + 1;

  // Skip empty domain
  if (*domain == '\0') {
    return -1;
  }

  size_t i = 0;
  while (domain[i] != '\0' && domain[i] != ' ' && domain[i] != '\t' &&
         domain[i] != '\r' && domain[i] != '\n') {
    if (i >= out_len - 1) {
      // Domain too long for buffer
      return -1;
    }
    out[i] = (char)tolower((unsigned char)domain[i]);
    i++;
  }
  out[i] = '\0';

  if (i == 0) {
    return -1;
  }

  return 0;
}

int email_dns_can_receive(const char *domain) {
  if (domain == NULL || *domain == '\0') {
    return 0;
  }

  // Respect EMAIL_DNS_CHECK=0 to skip lookups in dev/test
  const char *dns_check_env = getenv("EMAIL_DNS_CHECK");
  if (dns_check_env != NULL && dns_check_env[0] == '0') {
    return 1;
  }

  // Check MX record first
  unsigned char buf[DNS_BUF_LEN];
  int mx_len = res_search(domain, C_IN, T_MX, buf, sizeof(buf));
  if (mx_len > 0) {
    printf("DNS MX found for %s\n", domain);
    return 1;
  }

  // Fall back to A record
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int rc = getaddrinfo(domain, NULL, &hints, &res);
  if (rc == 0) {
    freeaddrinfo(res);
    printf("DNS A found for %s\n", domain);
    return 1;
  }

  // Fall back to AAAA record
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;

  rc = getaddrinfo(domain, NULL, &hints, &res);
  if (rc == 0) {
    freeaddrinfo(res);
    printf("DNS AAAA found for %s\n", domain);
    return 1;
  }

  printf("No DNS record found for %s\n", domain);
  return 0;
}
