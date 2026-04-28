/**
 * @file lib/email.c
 * @brief Implementation of send_mail() using the Resend HTTP API via libcurl.
 */

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int send_mail(const char *to, const char *subject, const char *html) {
  const char *api_key = getenv("RESEND_API_KEY");
  if (!api_key) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("RESEND API KEY NOT SET"));
    return 1;
  }

  struct MemoryStruct chunk;

  chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;           /* no data at this point */

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR INITIALIZE CURL"));
    return 1;
  }

  const char *from = getenv("MAIL_FROM");
  if (!from)
    from = "datenow@lauradurieux.dev";

  cJSON *body_json = cJSON_CreateObject();
  cJSON_AddStringToObject(body_json, "from", from);
  cJSON_AddStringToObject(body_json, "subject", subject);
  cJSON_AddStringToObject(body_json, "html", html);

  cJSON *to_arr = cJSON_CreateArray();
  cJSON *to_item = cJSON_CreateString(to);
  cJSON_AddItemToArray(to_arr, to_item);
  cJSON_AddItemToObject(body_json, "to", to_arr);

  char auth_header[256];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           api_key);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-type: application/json");
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.resend.com/emails");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cJSON_PrintUnformatted(body_json));
  cJSON_Delete(body_json);

  /* send all data to this function  */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  CURLcode is_email_sent = curl_easy_perform(curl);
  if (is_email_sent != CURLE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR WHILE SENDING EMAIL"));
    fprintf(stderr, "curl error: %s\n", errbuf);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (chunk.memory)
      free(chunk.memory);

    return 1;
  }

  long res_status_code;
  int response_info =
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_status_code);
  if (!is_email_sent && res_status_code) {
    printf("test: %ld\n", res_status_code);
    printf("%lu bytes retrieved\n", (long)chunk.size);
    printf("response: %s\n", chunk.memory);

    if (res_status_code != 200 && res_status_code != 201) {
      const cJSON *message = NULL;

      // Get error message from Resend response
      cJSON *res_json = cJSON_Parse(chunk.memory);
      if (res_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
          fprintf(stderr, "Error before: %s\n", error_ptr);
        }
      } else {
        message = cJSON_GetObjectItemCaseSensitive(res_json, "message");
        if (cJSON_IsString(message) && (message->valuestring != NULL)) {
          printf("Response error: \"%s\"\n", message->valuestring);
        }
      }

      cJSON_Delete(res_json);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("RESEND ERROR"));

      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);

      if (chunk.memory)
        free(chunk.memory);

      return 1;
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (chunk.memory)
    free(chunk.memory);

  return 0;
}
