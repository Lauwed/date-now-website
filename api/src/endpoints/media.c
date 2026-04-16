#include <endpoints/auth.h>
#include <endpoints/media.h>
#include <enums.h>
#include <errno.h>
#include <lib/mongoose.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <sql/media.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <structs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils.h>
#include <vips/vips.h>

#define TEXT_ALT_REQUIRED_MESSAGE "textAlternatif is required and must not be empty."
#define FILE_REQUIRED_MESSAGE "file is required."
#define FILE_TOO_LARGE_MESSAGE "File must not exceed 5MB."
#define FILE_NOT_IMAGE_MESSAGE "File must be an image."

#define MAX_UPLOAD_SIZE (5 * 1024 * 1024)
#define UPLOAD_DIR "/var/www/uploads/media"

static int is_image_data(const char *data, size_t len) {
  if (len < 4) return 0;
  const unsigned char *d = (const unsigned char *)data;
  /* JPEG */
  if (d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return 1;
  /* PNG */
  if (d[0] == 0x89 && d[1] == 0x50 && d[2] == 0x4E && d[3] == 0x47) return 1;
  /* GIF */
  if (d[0] == 0x47 && d[1] == 0x49 && d[2] == 0x46) return 1;
  /* WebP: RIFF....WEBP */
  if (len >= 12 && d[0] == 0x52 && d[1] == 0x49 && d[2] == 0x46 &&
      d[3] == 0x46 && d[8] == 0x57 && d[9] == 0x45 && d[10] == 0x42 &&
      d[11] == 0x50)
    return 1;
  /* BMP */
  if (d[0] == 0x42 && d[1] == 0x4D) return 1;
  /* TIFF LE */
  if (d[0] == 0x49 && d[1] == 0x49 && d[2] == 0x2A && d[3] == 0x00) return 1;
  /* TIFF BE */
  if (d[0] == 0x4D && d[1] == 0x4D && d[2] == 0x00 && d[3] == 0x2A) return 1;
  return 0;
}

static void remove_media_dir(unsigned id) {
  char img_path[512], thumb_path[512], dir_path[256];
  snprintf(dir_path, sizeof(dir_path), UPLOAD_DIR "/%u", id);
  snprintf(img_path, sizeof(img_path), UPLOAD_DIR "/%u/image.webp", id);
  snprintf(thumb_path, sizeof(thumb_path), UPLOAD_DIR "/%u/thumb.webp", id);
  unlink(img_path);
  unlink(thumb_path);
  rmdir(dir_path);
}

void send_medias_res(struct mg_connection *c, struct mg_http_message *msg,
                     struct error_reply *error_reply, const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA LIST ==="));

    int total = get_medias_len();

    struct media **medias = NULL;
    if (total > 0) {
      medias = malloc((size_t)total * sizeof(struct media *));
      query_code = get_medias((size_t)total, medias);
      if (query_code != 0) {
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING MEDIAS"));
        HANDLE_QUERY_CODE;
        free(medias);
        return;
      }
    }

    char *data = medias_to_json(medias, (size_t)total);
    struct list_reply *reply = malloc(sizeof(struct list_reply));
    reply->data = data;
    reply->count = total;
    reply->total = total;
    reply->total_pages = 0;
    reply->page = -1;
    reply->page_size = 0;
    list_reply_to_json(reply);

    mg_http_reply(c, 200, JSON_HEADER, "%s\n", reply->json);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIAS SUCCESSFULLY SENT ==="));

    if (medias != NULL) {
      for (int i = 0; i < total; i++) {
        free_media(medias[i]);
      }
      free(medias);
    }
    if (total > 0) free(data);
    free(reply->json);
    free(reply);

  } else if (mg_match(msg->method, mg_str("POST"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== POST MEDIA UPLOAD ==="));

    /* Auth check */
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    /* Parse multipart */
    struct mg_http_part text_part, file_part;
    memset(&text_part, 0, sizeof(text_part));
    memset(&file_part, 0, sizeof(file_part));
    int found_text = 0, found_file = 0;

    struct mg_http_part part;
    size_t ofs = 0;
    while ((ofs = mg_http_next_multipart(msg->body, ofs, &part)) > 0) {
      if (mg_strcmp(part.name, mg_str("textAlternatif")) == 0) {
        text_part = part;
        found_text = 1;
      } else if (mg_strcmp(part.name, mg_str("file")) == 0) {
        file_part = part;
        found_file = 1;
      }
    }

    /* Validate textAlternatif */
    if (!found_text || text_part.body.len == 0) {
      ERROR_REPLY_400(TEXT_ALT_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(TEXT_ALT_REQUIRED_MESSAGE));
      return;
    }

    /* Validate file */
    if (!found_file || file_part.body.len == 0) {
      ERROR_REPLY_400(FILE_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(FILE_REQUIRED_MESSAGE));
      return;
    }

    /* Check file size */
    if (file_part.body.len > MAX_UPLOAD_SIZE) {
      ERROR_REPLY_400(FILE_TOO_LARGE_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(FILE_TOO_LARGE_MESSAGE));
      return;
    }

    /* Check magic bytes */
    if (!is_image_data(file_part.body.buf, file_part.body.len)) {
      ERROR_REPLY_400(FILE_NOT_IMAGE_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(FILE_NOT_IMAGE_MESSAGE));
      return;
    }

    /* Copy textAlternatif (strip trailing whitespace/newlines) */
    char *alt_text = strndup(text_part.body.buf, text_part.body.len);
    size_t alt_len = strlen(alt_text);
    while (alt_len > 0 &&
           (alt_text[alt_len - 1] == '\r' || alt_text[alt_len - 1] == '\n' ||
            alt_text[alt_len - 1] == ' '))
      alt_text[--alt_len] = '\0';

    if (alt_len == 0) {
      free(alt_text);
      ERROR_REPLY_400(TEXT_ALT_REQUIRED_MESSAGE);
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(TEXT_ALT_REQUIRED_MESSAGE));
      return;
    }

    /* Insert into DB to get the auto-incremented ID */
    struct media *m = malloc(sizeof(struct media));
    m->id = 0;
    m->alternative_text = alt_text;
    m->url = NULL;
    m->width = 0.0;
    m->height = 0.0;

    query_code = add_media(m);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR INSERTING MEDIA"));
      HANDLE_QUERY_CODE;
      free(alt_text);
      free(m);
      return;
    }

    unsigned media_id = m->id;

    /* Create upload directory */
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), UPLOAD_DIR "/%u", media_id);
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING MEDIA DIR"));
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }

    /* Write raw upload to a temp file */
    char tmp_path[] = "/tmp/media_upload_XXXXXX";
    int tmp_fd = mkstemp(tmp_path);
    if (tmp_fd < 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR CREATING TEMP FILE"));
      remove_media_dir(media_id);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }
    write(tmp_fd, file_part.body.buf, file_part.body.len);
    close(tmp_fd);

    /* Build output paths */
    char img_path[512], thumb_path[512];
    snprintf(img_path, sizeof(img_path), UPLOAD_DIR "/%u/image.webp", media_id);
    snprintf(thumb_path, sizeof(thumb_path), UPLOAD_DIR "/%u/thumb.webp",
             media_id);

    /* Convert to WebP at max 1200px width using libvips */
    VipsImage *img = NULL;
    if (vips_thumbnail(tmp_path, &img, 1200, "size", VIPS_SIZE_DOWN, NULL) !=
        0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("VIPS THUMBNAIL FAILED"));
      unlink(tmp_path);
      remove_media_dir(media_id);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }

    int img_width = vips_image_get_width(img);
    int img_height = vips_image_get_height(img);

    if (vips_webpsave(img, img_path, NULL) != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("VIPS WEBPSAVE FAILED"));
      g_object_unref(img);
      unlink(tmp_path);
      remove_media_dir(media_id);
      delete_media((int)media_id);
      ERROR_REPLY_500;
      free(alt_text);
      free(m);
      return;
    }
    g_object_unref(img);

    /* Generate 300px thumbnail (non-fatal if it fails) */
    VipsImage *tn = NULL;
    if (vips_thumbnail(tmp_path, &tn, 300, "size", VIPS_SIZE_DOWN, NULL) == 0) {
      vips_webpsave(tn, thumb_path, NULL);
      g_object_unref(tn);
    }

    unlink(tmp_path);

    /* Update DB with final URL and dimensions */
    char url[256];
    snprintf(url, sizeof(url), "/uploads/media/%u/image.webp", media_id);

    query_code =
        update_media_file((int)media_id, url, (double)img_width, (double)img_height);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR UPDATING MEDIA FILE"));
      HANDLE_QUERY_CODE;
      free(alt_text);
      free(m);
      return;
    }

    /* Re-fetch from DB and return 201 */
    struct media *created = malloc(sizeof(struct media));
    created->id = 0;
    created->alternative_text = NULL;
    created->url = NULL;
    created->width = 0.0;
    created->height = 0.0;

    query_code = get_media(created, (int)media_id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR FETCHING CREATED MEDIA"));
      HANDLE_QUERY_CODE;
      free(created);
      free(alt_text);
      free(m);
      return;
    }

    char *result = media_to_json(created);
    mg_http_reply(c, 201, JSON_HEADER, "%s\n", result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY CREATED ==="));

    free(result);
    free_media(created);
    free(alt_text);
    free(m);

  } else {
    ERROR_REPLY_405;
  }
}

void send_media_res(struct mg_connection *c, struct mg_http_message *msg,
                    int id, struct error_reply *error_reply,
                    const char *secret) {
  int query_code;
  error_reply = malloc(sizeof(struct error_reply));

  int exists = media_exists(id);
  if (!exists) {
    ERROR_REPLY_404;
    fprintf(stderr, TERMINAL_ERROR_MESSAGE("MEDIA NOT FOUND"));
    return;
  }

  if (mg_match(msg->method, mg_str("GET"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== GET MEDIA ==="));

    struct media *m = malloc(sizeof(struct media));
    m->id = 0;
    m->alternative_text = NULL;
    m->url = NULL;
    m->width = 0.0;
    m->height = 0.0;

    query_code = get_media(m, id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR RETRIEVING MEDIA"));
      HANDLE_QUERY_CODE;
      free(m);
      return;
    }

    char *result = media_to_json(m);
    mg_http_reply(c, 200, JSON_HEADER, "%s\n", result);
    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY SENT ==="));

    free(result);
    free_media(m);

  } else if (mg_match(msg->method, mg_str("DELETE"), NULL)) {
    printf(TERMINAL_ENDPOINT_MESSAGE("=== DELETE MEDIA ==="));

    /* Auth check */
    int user_logged = 0;
    is_user_logged(c, msg, error_reply, secret, &user_logged);
    if (user_logged == 0) {
      ERROR_REPLY_401;
      fprintf(stderr, TERMINAL_ERROR_MESSAGE(UNAUTHORIZED_MESSAGE));
      return;
    }

    /* Remove files from disk */
    remove_media_dir((unsigned)id);

    /* Remove from DB */
    query_code = delete_media(id);
    if (query_code != 0) {
      fprintf(stderr, TERMINAL_ERROR_MESSAGE("ERROR DELETING MEDIA"));
      ERROR_REPLY_500;
      return;
    }

    printf(TERMINAL_SUCCESS_MESSAGE("=== MEDIA SUCCESSFULLY DELETED ==="));
    mg_http_reply(c, 200, JSON_HEADER,
                  "{ \"message\": \"Media successfully deleted\" }");

  } else {
    ERROR_REPLY_405;
  }
}
