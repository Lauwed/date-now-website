/**
 * @file lib/blob.c
 * @brief Implementation of blob_upload()/blob_delete() using the Vercel Blob
 *        REST API via libcurl.
 */

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOB_API_BASE "https://blob.vercel-storage.com"

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
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int blob_upload(const char *pathname, const unsigned char *data, size_t len,
                const char *content_type, char **out_url) {
  const char *token = getenv("BLOB_READ_WRITE_TOKEN");
  if (!token) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BLOB_READ_WRITE_TOKEN NOT SET"));
    return 1;
  }

  struct MemoryStruct chunk;
  chunk.memory = malloc(1);
  chunk.size = 0;

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR INITIALIZE CURL"));
    free(chunk.memory);
    return 1;
  }

  char url[512];
  snprintf(url, sizeof(url), "%s/%s", BLOB_API_BASE, pathname);

  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           token);

  char content_type_header[256];
  snprintf(content_type_header, sizeof(content_type_header),
           "Content-Type: %s", content_type ? content_type : "application/octet-stream");

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, auth_header);
  headers = curl_slist_append(headers, content_type_header);
  headers = curl_slist_append(headers, "x-api-version: 7");
  /* Pathnames are already unique (per-upload UUID dir) — keep them
   * predictable rather than letting Blob append a random suffix. */
  headers = curl_slist_append(headers, "x-add-random-suffix: 0");

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR WHILE UPLOADING TO BLOB"));
    fprintf(stderr, "curl error: %s\n", errbuf);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 1;
  }

  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

  int ret = 1;
  if (status_code == 200 && chunk.memory) {
    cJSON *res_json = cJSON_Parse(chunk.memory);
    if (res_json != NULL) {
      const cJSON *url_item =
          cJSON_GetObjectItemCaseSensitive(res_json, "url");
      if (cJSON_IsString(url_item) && url_item->valuestring != NULL) {
        *out_url = strdup(url_item->valuestring);
        ret = 0;
      }
      cJSON_Delete(res_json);
    }
  }

  if (ret != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BLOB UPLOAD ERROR"));
    if (chunk.memory)
      fprintf(stderr, "response (%ld): %s\n", status_code, chunk.memory);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(chunk.memory);

  return ret;
}

int blob_delete(const char * const *urls, size_t count) {
  const char *token = getenv("BLOB_READ_WRITE_TOKEN");
  if (!token) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BLOB_READ_WRITE_TOKEN NOT SET"));
    return 1;
  }

  cJSON *body_json = cJSON_CreateObject();
  cJSON *urls_arr = cJSON_CreateArray();
  for (size_t i = 0; i < count; i++) {
    if (urls[i] == NULL)
      continue;
    cJSON_AddItemToArray(urls_arr, cJSON_CreateString(urls[i]));
  }
  cJSON_AddItemToObject(body_json, "urls", urls_arr);

  if (cJSON_GetArraySize(urls_arr) == 0) {
    cJSON_Delete(body_json);
    return 0;
  }

  char *body = cJSON_PrintUnformatted(body_json);
  cJSON_Delete(body_json);

  struct MemoryStruct chunk;
  chunk.memory = malloc(1);
  chunk.size = 0;

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR INITIALIZE CURL"));
    free(body);
    free(chunk.memory);
    return 1;
  }

  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           token);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, auth_header);
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "x-api-version: 7");

  char url[256];
  snprintf(url, sizeof(url), "%s/delete", BLOB_API_BASE);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  CURLcode res = curl_easy_perform(curl);
  free(body);
  if (res != CURLE_OK) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR WHILE DELETING FROM BLOB"));
    fprintf(stderr, "curl error: %s\n", errbuf);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 1;
  }

  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

  int ret = (status_code >= 200 && status_code < 300) ? 0 : 1;
  if (ret != 0) {
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("BLOB DELETE ERROR"));
    if (chunk.memory)
      fprintf(stderr, "response (%ld): %s\n", status_code, chunk.memory);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(chunk.memory);

  return ret;
}
