#include <curl/curl.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <stdio.h>
#include <stdlib.h>

int send_mail(const char *to, const char *subject, const char *html) {
  const char *api_key = getenv("RESEND_API_KEY");
  if (!api_key) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("RESEND API KEY NOT SET"));
    return 1;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR INITIALIZE CURL"));
    return 1;
  }

  char body[1024];
  snprintf(body, sizeof(body),
           "{\"from\":\"datenow@lauradurieux.dev\","
           "\"to\":[\"%s\"],"
           "\"subject\":\"%s\","
           "\"html\":\"%s\"}",
           to, subject, html);

  char auth_header[256];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           api_key);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-type: application/json");
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.resend.com/emails");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  CURLcode is_email_sent = curl_easy_perform(curl);
  if (is_email_sent != CURLE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR WHILE SENDING EMAIL"));
    fprintf(stderr, "curl error: %s\n", errbuf);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return 1;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return 0;
}
