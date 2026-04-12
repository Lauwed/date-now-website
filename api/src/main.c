#include <endpoints/auth.h>
#include <endpoints/issue.h>
#include <endpoints/issue_author.h>
#include <endpoints/issue_sponsor.h>
#include <endpoints/issue_tag.h>
#include <endpoints/sponsor.h>
#include <endpoints/tag.h>
#include <endpoints/user.h>
#include <endpoints/view.h>
#include <lib/mongoose.h>
#include <sqlite3.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

sqlite3 *db;

void clean_db() { sqlite3_close(db); }

void sigTerm(int code) {
  printf(">>> SIGTERM received [%d]\n", code);
  clean_db();

  exit(EXIT_SUCCESS);
}

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *http_msg = (struct mg_http_message *)ev_data;
    struct mg_str endpoint_cap[2];

    if (mg_match(http_msg->uri, mg_str("/api/#"), endpoint_cap)) {
      struct error_reply *error_reply = NULL;

      // JWT Secret
      const char *secret = getenv("JWT_SECRET");
      if (!secret) {
        mg_http_reply(c, 500, JSON_HEADER,
                      "{\"code\": 500, \"error\": \"Internal Error\"}");
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("SECRET JWT NOT SET"));
        exit(1);
      }

      // Check if user logged

      printf("endpoint:  %.*s\n", (int)endpoint_cap[0].len,
             endpoint_cap[0].buf);

      if (mg_match(endpoint_cap[0], mg_str("auth#"), NULL)) {
        struct mg_str caps[2];

        if (mg_match(endpoint_cap[0], mg_str("auth/subscribe"), caps)) {
          send_subscription_mail(c, http_msg, error_reply, secret);
        }
        if (mg_match(endpoint_cap[0], mg_str("auth/subscribe/confirm"), caps)) {
          subscribe_user(c, http_msg, error_reply, secret);
        }
        if (mg_match(endpoint_cap[0], mg_str("auth/register"), caps)) {
          register_user(c, http_msg, error_reply);
        }
        if (mg_match(endpoint_cap[0], mg_str("auth/login"), caps)) {
          send_login_mail(c, http_msg, error_reply);
        }
        if (mg_match(endpoint_cap[0], mg_str("auth/login/totp"), caps)) {
          login_user(c, http_msg, error_reply);
        }

        mg_http_reply(c, 404, JSON_HEADER,
                      "{\"code\": 404, \"error\": \"Not found\"}");
        return;
      }
      if (mg_match(endpoint_cap[0], mg_str("user#"), NULL)) {
        struct mg_str caps[2];

        if (mg_match(endpoint_cap[0], mg_str("user/*"), caps)) {
          int id;
          int id_parsed = mg_str_to_num(caps[0], 10, &id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"ID is not a number.\" }");
            return;
          }

          send_user_res(c, http_msg, id, error_reply);
        } else if (mg_strcmp(endpoint_cap[0], mg_str("user")) == 0) {
          send_users_res(c, http_msg, error_reply);
        }

        mg_http_reply(c, 404, JSON_HEADER,
                      "{\"code\": 404, \"error\": \"Not found\"}");
        return;
      } else if (mg_match(endpoint_cap[0], mg_str("tag#"), NULL)) {
        struct mg_str caps[2];

        if (mg_match(endpoint_cap[0], mg_str("tag/*"), caps)) {
          char *name = malloc(caps[0].len + 1);
          int name_decoded_len =
              mg_url_decode(caps[0].buf, caps[0].len, name, caps[0].len + 1, 0);
          if (name_decoded_len <= 0) {
            mg_http_reply(c, 400, JSON_HEADER,
                          "{\"code\": 400, \"error\": \"Wrong name\"}");
            return;
          }

          send_tag_res(c, http_msg, name, error_reply);
          free(name);
        } else if (mg_strcmp(endpoint_cap[0], mg_str("tag")) == 0) {
          send_tags_res(c, http_msg, error_reply);
        }

        mg_http_reply(c, 404, JSON_HEADER,
                      "{\"code\": 404, \"error\": \"Not found\"}");
        return;
      } else if (mg_match(endpoint_cap[0], mg_str("sponsor#"), NULL)) {
        struct mg_str caps[2];

        if (mg_match(endpoint_cap[0], mg_str("sponsor/*"), caps)) {
          char *name = malloc(caps[0].len + 1);
          int name_decoded_len =
              mg_url_decode(caps[0].buf, caps[0].len, name, caps[0].len + 1, 0);
          if (name_decoded_len <= 0) {
            mg_http_reply(c, 400, "", "%m", MG_ESC("error"));
            return;
          }

          send_sponsor_res(c, http_msg, name, error_reply);
          free(name);
        } else if (mg_strcmp(endpoint_cap[0], mg_str("sponsor")) == 0) {
          send_sponsors_res(c, http_msg, error_reply);
        }

        mg_http_reply(c, 404, JSON_HEADER,
                      "{\"code\": 404, \"error\": \"Not found\"}");
        return;
      } else if (mg_match(endpoint_cap[0], mg_str("view#"), NULL)) {
        printf("VIEWS PUTAIN\n");
        printf("%d\n", mg_strcmp(endpoint_cap[0], mg_str("view")) == 0);
        if (mg_strcmp(endpoint_cap[0], mg_str("view")) == 0) {
          send_views_res(c, http_msg, error_reply);
        }

        mg_http_reply(c, 404, JSON_HEADER,
                      "{\"code\": 404, \"error\": \"Not found\"}");
        return;
      } else if (mg_match(endpoint_cap[0], mg_str("issue#"), NULL)) {
        struct mg_str caps[3];
        printf("issues\n");

        if (mg_match(endpoint_cap[0], mg_str("issue/*/tag"), caps)) {
          printf("issue tags\n");

          int issue_id;
          int id_parsed = mg_str_to_num(caps[0], 10, &issue_id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"Issue ID is not a number.\" }");
            return;
          }

          send_issue_tags_res(c, http_msg, issue_id, error_reply);
        }
        if (mg_match(endpoint_cap[0], mg_str("issue/*/tag/*"), caps)) {
          printf("issue tag\n");

          int issue_id;
          int id_parsed = mg_str_to_num(caps[0], 10, &issue_id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"Issue ID is not a number.\" }");
            return;
          }

          char *name = malloc(caps[1].len + 1);
          int name_decoded_len =
              mg_url_decode(caps[1].buf, caps[1].len, name, caps[1].len + 1, 0);
          if (name_decoded_len <= 0) {
            mg_http_reply(c, 400, JSON_HEADER,
                          "{\"code\": 400, \"error\": \"Wrong name\"}");
            return;
          }

          send_issue_tag_res(c, http_msg, issue_id, name, error_reply);
        }

        if (mg_match(endpoint_cap[0], mg_str("issue/*/author"), caps)) {
          printf("issue authors\n");
          int issue_id;
          int id_parsed = mg_str_to_num(caps[0], 10, &issue_id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"Issue ID is not a number.\" }");
            return;
          }

          send_issue_authors_res(c, http_msg, issue_id, error_reply);
        }
        if (mg_match(endpoint_cap[0], mg_str("issue/*/author/*"), caps)) {
          printf("issue author\n");
          int issue_id;
          int id_parsed = mg_str_to_num(caps[0], 10, &issue_id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"Issue ID is not a number.\" }");
            return;
          }

          int id;
          id_parsed = mg_str_to_num(caps[1], 10, &id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"ID is not a number.\" }");
            return;
          }

          send_issue_author_res(c, http_msg, issue_id, id, error_reply);
        }

        if (mg_match(endpoint_cap[0], mg_str("issue/*/sponsor"), caps)) {
          printf("issue sponsors\n");
          int issue_id;
          int id_parsed = mg_str_to_num(caps[0], 10, &issue_id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"Issue ID is not a number.\" }");
            return;
          }

          send_issue_sponsors_res(c, http_msg, issue_id, error_reply);
        }
        if (mg_match(endpoint_cap[0], mg_str("issue/*/sponsor/*"), caps)) {
          printf("issue sponsor\n");
          int issue_id;
          int id_parsed = mg_str_to_num(caps[0], 10, &issue_id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"Issue ID is not a number.\" }");
            return;
          }

          char *name = malloc(caps[1].len + 1);
          int name_decoded_len =
              mg_url_decode(caps[1].buf, caps[1].len, name, caps[1].len + 1, 0);
          if (name_decoded_len <= 0) {
            mg_http_reply(c, 400, JSON_HEADER,
                          "{\"code\": 400, \"error\": \"Wrong name\"}");
            return;
          }

          send_issue_sponsor_res(c, http_msg, issue_id, name, error_reply);
        }

        if (mg_match(endpoint_cap[0], mg_str("issue/*"), caps)) {
          int id;
          int id_parsed = mg_str_to_num(caps[0], 10, &id, sizeof(int));
          if (!id_parsed) {
            mg_http_reply(
                c, 400, JSON_HEADER,
                "{ \"code\": 400, \"error\": \"ID is not a number.\" }");
            return;
          }

          send_issue_res(c, http_msg, id, error_reply);
        } else if (mg_strcmp(endpoint_cap[0], mg_str("issue")) == 0) {
          send_issues_res(c, http_msg, error_reply);
        }

        mg_http_reply(c, 404, JSON_HEADER,
                      "{\"code\": 404, \"error\": \"Not found\"}");
        return;
      }

      if (error_reply != NULL) {
        free(error_reply->json);
        free(error_reply->message);
      }
      free(error_reply);
    } else {
      struct mg_http_serve_opts opts = {.root_dir = ".", .fs = &mg_fs_posix};
      mg_http_serve_dir(c, http_msg, &opts);
    }
  } else if (ev == MG_EV_ERROR) {
    mg_http_reply(c, 500, JSON_HEADER,
                  "{\"code\": 500, \"error\": \"Internal Error\"}");
  }
}

int main() {
  printf("\n\n");
  printf(ANSI_COLOR_RGB(
      221, 36,
      118) " ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ " ANSI_RESET_ALL
           "\n");
  printf(ANSI_COLOR_RGB(
      231, 50,
      97) "||D |||A |||T |||E |||. |||N |||O |||W |||( |||) ||" ANSI_RESET_ALL
          "\n");
  printf(ANSI_COLOR_RGB(
      246, 69,
      67) "||__|||__|||__|||__|||__|||__|||__|||__|||__|||__||" ANSI_RESET_ALL
          "\n");
  printf(ANSI_COLOR_RGB(255, 81, 47) "|/__\\|/__\\|/__\\|/__\\|/__\\|/__\\|/"
                                     "__\\|/__\\|/__\\|/__\\|" ANSI_RESET_ALL
                                     "\n");
  printf("\n\n");

  printf("===\tDB - opening...\t===\n");

  int atexit_return_code = atexit(clean_db);
  if (atexit_return_code != 0) {
    fprintf(stderr, "Couldn't correctly add atexit function. Error code: %d\n",
            atexit_return_code);
    return EXIT_FAILURE;
  }

  signal(SIGTERM, &sigTerm);

  int db_return_code = sqlite3_open("uwu.db", &db);
  if (SQLITE_OK != db_return_code) {
    fprintf(stderr, "DB - Can't open Database UwU: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return EXIT_FAILURE;
  }

  printf("===\tDB - successfully opened!\t===\n");

  struct mg_mgr mgr;
  mg_log_set(MG_LL_DEBUG);
  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "http://0.0.0.0:8000", ev_handler, NULL);

  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }

  return EXIT_SUCCESS;
}
