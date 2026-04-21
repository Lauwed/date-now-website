/**
 * @file main.c
 * @brief Entry point: initialises SQLite and ImageMagick, opens the database,
 *        registers signal handlers, and runs the Mongoose HTTP event loop with
 *        a table-driven URL router.
 */

#include <MagickWand/MagickWand.h>
#include <endpoints/auth.h>
#include <endpoints/issue.h>
#include <endpoints/issue_author.h>
#include <endpoints/issue_sponsor.h>
#include <endpoints/issue_tag.h>
#include <endpoints/media.h>
#include <endpoints/sponsor.h>
#include <endpoints/tag.h>
#include <endpoints/user.h>
#include <endpoints/view.h>
#include <lib/mongoose.h>
#include <lib/rate_limiter.h>
#include <macros/colors.h>
#include <macros/endpoints.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <structs.h>
#include <utils.h>

sqlite3 *db;
char g_json_header[512];

static void clean_db(void) { sqlite3_close(db); }

static void handle_shutdown(int code) {
  printf(">>> signal %d received — shutting down\n", code);
  clean_db();
  exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------
 * Route dispatch table
 * -------------------------------------------------------------------------
 * Each entry maps a URL pattern to a handler.  Patterns use Mongoose syntax:
 *   *  — one path segment (no '/')
 *   #  — any characters including '/'
 * More-specific patterns must appear before less-specific ones.
 * ------------------------------------------------------------------------- */

typedef void (*route_fn)(struct mg_connection *c, struct mg_http_message *msg,
                         struct mg_str *caps, struct error_reply *er,
                         const char *secret);

struct route_entry {
  const char *pattern;
  route_fn    handler;
  int         rl_max;     /* 0 = no per-route rate limit */
  int         rl_window;
};

/* ---- helpers ---- */

static void reply_bad_id(struct mg_connection *c) {
  mg_http_reply(c, 400, JSON_HEADER,
                "{\"code\":400,\"error\":\"ID is not a number.\"}");
}

static void reply_bad_name(struct mg_connection *c) {
  mg_http_reply(c, 400, JSON_HEADER,
                "{\"code\":400,\"error\":\"Wrong name.\"}");
}

/* URL-decode caps[n] into a malloc'd NUL-terminated string; returns NULL on
 * failure (caller must NOT free on NULL return). */
static char *decode_name(struct mg_str cap) {
  char *buf = malloc(cap.len + 1);
  int n = mg_url_decode(cap.buf, cap.len, buf, (int)cap.len + 1, 0);
  if (n <= 0) {
    free(buf);
    return NULL;
  }
  return buf;
}

/* ---- auth route handlers ---- */

static void r_login_totp(struct mg_connection *c, struct mg_http_message *msg,
                         struct mg_str *caps, struct error_reply *er,
                         const char *secret) {
  (void)caps;
  login_user(c, msg, er, secret);
}

static void r_subscribe(struct mg_connection *c, struct mg_http_message *msg,
                        struct mg_str *caps, struct error_reply *er,
                        const char *secret) {
  (void)caps;
  send_subscription_mail(c, msg, er, secret);
}

static void r_subscribe_confirm(struct mg_connection *c,
                                struct mg_http_message *msg,
                                struct mg_str *caps, struct error_reply *er,
                                const char *secret) {
  (void)caps;
  subscribe_user(c, msg, er, secret);
}

static void r_register(struct mg_connection *c, struct mg_http_message *msg,
                       struct mg_str *caps, struct error_reply *er,
                       const char *secret) {
  (void)caps;
  register_user(c, msg, er, secret);
}

static void r_seed(struct mg_connection *c, struct mg_http_message *msg,
                   struct mg_str *caps, struct error_reply *er,
                   const char *secret) {
  (void)caps;
  generate_totpseed_user(c, msg, er, secret);
}

static void r_login_mail(struct mg_connection *c, struct mg_http_message *msg,
                         struct mg_str *caps, struct error_reply *er,
                         const char *secret) {
  (void)caps;
  send_login_mail(c, msg, er, secret);
}

/* ---- user route handlers ---- */

static void r_users(struct mg_connection *c, struct mg_http_message *msg,
                    struct mg_str *caps, struct error_reply *er,
                    const char *secret) {
  (void)caps;
  send_users_res(c, msg, er, secret);
}

static void r_user(struct mg_connection *c, struct mg_http_message *msg,
                   struct mg_str *caps, struct error_reply *er,
                   const char *secret) {
  int id;
  if (!mg_str_to_num(caps[0], 10, &id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_user_res(c, msg, id, er, secret);
}

/* ---- tag route handlers ---- */

static void r_tags(struct mg_connection *c, struct mg_http_message *msg,
                   struct mg_str *caps, struct error_reply *er,
                   const char *secret) {
  (void)caps;
  send_tags_res(c, msg, er, secret);
}

static void r_tag(struct mg_connection *c, struct mg_http_message *msg,
                  struct mg_str *caps, struct error_reply *er,
                  const char *secret) {
  char *name = decode_name(caps[0]);
  if (!name) {
    reply_bad_name(c);
    return;
  }
  send_tag_res(c, msg, name, er, secret);
  free(name);
}

/* ---- sponsor route handlers ---- */

static void r_sponsors(struct mg_connection *c, struct mg_http_message *msg,
                       struct mg_str *caps, struct error_reply *er,
                       const char *secret) {
  (void)caps;
  send_sponsors_res(c, msg, er, secret);
}

static void r_sponsor(struct mg_connection *c, struct mg_http_message *msg,
                      struct mg_str *caps, struct error_reply *er,
                      const char *secret) {
  char *name = decode_name(caps[0]);
  if (!name) {
    reply_bad_name(c);
    return;
  }
  send_sponsor_res(c, msg, name, er, secret);
  free(name);
}

/* ---- view route handlers ---- */

static void r_views(struct mg_connection *c, struct mg_http_message *msg,
                    struct mg_str *caps, struct error_reply *er,
                    const char *secret) {
  (void)caps;
  send_views_res(c, msg, er, secret);
}

/* ---- media route handlers ---- */

static void r_medias(struct mg_connection *c, struct mg_http_message *msg,
                     struct mg_str *caps, struct error_reply *er,
                     const char *secret) {
  (void)caps;
  send_medias_res(c, msg, er, secret);
}

static void r_media(struct mg_connection *c, struct mg_http_message *msg,
                    struct mg_str *caps, struct error_reply *er,
                    const char *secret) {
  int id;
  if (!mg_str_to_num(caps[0], 10, &id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_media_res(c, msg, id, er, secret);
}

/* ---- issue route handlers ---- */

static void r_issues(struct mg_connection *c, struct mg_http_message *msg,
                     struct mg_str *caps, struct error_reply *er,
                     const char *secret) {
  (void)caps;
  send_issues_res(c, msg, er, secret);
}

static void r_issue(struct mg_connection *c, struct mg_http_message *msg,
                    struct mg_str *caps, struct error_reply *er,
                    const char *secret) {
  int id;
  if (!mg_str_to_num(caps[0], 10, &id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_issue_res(c, msg, id, er, secret);
}

static void r_issue_publish(struct mg_connection *c,
                            struct mg_http_message *msg, struct mg_str *caps,
                            struct error_reply *er, const char *secret) {
  int id;
  if (!mg_str_to_num(caps[0], 10, &id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  publish_issue_res(c, msg, id, er, secret);
}

static void r_issue_tags(struct mg_connection *c, struct mg_http_message *msg,
                         struct mg_str *caps, struct error_reply *er,
                         const char *secret) {
  int issue_id;
  if (!mg_str_to_num(caps[0], 10, &issue_id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_issue_tags_res(c, msg, issue_id, er, secret);
}

static void r_issue_tag(struct mg_connection *c, struct mg_http_message *msg,
                        struct mg_str *caps, struct error_reply *er,
                        const char *secret) {
  int issue_id;
  if (!mg_str_to_num(caps[0], 10, &issue_id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  char *name = decode_name(caps[1]);
  if (!name) {
    reply_bad_name(c);
    return;
  }
  send_issue_tag_res(c, msg, issue_id, name, er, secret);
  free(name);
}

static void r_issue_authors(struct mg_connection *c,
                            struct mg_http_message *msg, struct mg_str *caps,
                            struct error_reply *er, const char *secret) {
  int issue_id;
  if (!mg_str_to_num(caps[0], 10, &issue_id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_issue_authors_res(c, msg, issue_id, er, secret);
}

static void r_issue_author(struct mg_connection *c, struct mg_http_message *msg,
                           struct mg_str *caps, struct error_reply *er,
                           const char *secret) {
  int issue_id, id;
  if (!mg_str_to_num(caps[0], 10, &issue_id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  if (!mg_str_to_num(caps[1], 10, &id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_issue_author_res(c, msg, issue_id, id, er, secret);
}

static void r_issue_sponsors(struct mg_connection *c,
                             struct mg_http_message *msg, struct mg_str *caps,
                             struct error_reply *er, const char *secret) {
  int issue_id;
  if (!mg_str_to_num(caps[0], 10, &issue_id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  send_issue_sponsors_res(c, msg, issue_id, er, secret);
}

static void r_issue_sponsor(struct mg_connection *c,
                            struct mg_http_message *msg, struct mg_str *caps,
                            struct error_reply *er, const char *secret) {
  int issue_id;
  if (!mg_str_to_num(caps[0], 10, &issue_id, sizeof(int))) {
    reply_bad_id(c);
    return;
  }
  char *name = decode_name(caps[1]);
  if (!name) {
    reply_bad_name(c);
    return;
  }
  send_issue_sponsor_res(c, msg, issue_id, name, er, secret);
  free(name);
}

/* -------------------------------------------------------------------------
 * Route table — most-specific patterns first within each resource group.
 * ------------------------------------------------------------------------- */
static const struct route_entry routes[] = {
  /* auth — all share the prefix-level auth rate limit (applied in dispatch) */
  {"auth/login/totp",        r_login_totp,        RATE_LIMIT_TOTP_MAX, RATE_LIMIT_TOTP_WINDOW},
  {"auth/subscribe/confirm", r_subscribe_confirm, 0, 0},
  {"auth/subscribe",         r_subscribe,         0, 0},
  {"auth/register",          r_register,          0, 0},
  {"auth/seed",              r_seed,              0, 0},
  {"auth/login",             r_login_mail,        0, 0},
  // issue sub-resources before issue/* to avoid short-circuit
  {"issue/*/tag/*",          r_issue_tag,         0, 0},
  {"issue/*/tag",            r_issue_tags,        0, 0},
  {"issue/*/author/*",       r_issue_author,      0, 0},
  {"issue/*/author",         r_issue_authors,     0, 0},
  {"issue/*/sponsor/*",      r_issue_sponsor,     0, 0},
  {"issue/*/sponsor",        r_issue_sponsors,    0, 0},
  {"issue/*/publish",        r_issue_publish,     0, 0},
  {"issue/*",                r_issue,             0, 0},
  {"issue",                  r_issues,            0, 0},
  /* remaining resources */
  {"user/*",                 r_user,              0, 0},
  {"user",                   r_users,             0, 0},
  {"tag/*",                  r_tag,               0, 0},
  {"tag",                    r_tags,              0, 0},
  {"sponsor/*",              r_sponsor,           0, 0},
  {"sponsor",                r_sponsors,          0, 0},
  {"media/*",                r_media,             0, 0},
  {"media",                  r_medias,            0, 0},
  {"view",                   r_views,             0, 0},
  {NULL, NULL, 0, 0},
};

static void dispatch(struct mg_connection *c, struct mg_http_message *msg,
                     struct mg_str endpoint, struct error_reply *er,
                     const char *secret) {
  // Prefix-level auth rate limit — applies to every auth/* route
  if (mg_match(endpoint, mg_str("auth#"), NULL) &&
      rate_limit_check(&c->rem, RATE_LIMIT_AUTH_MAX, RATE_LIMIT_AUTH_WINDOW)) {
    ERROR_REPLY_429;
    return;
  }

  for (size_t i = 0; routes[i].pattern != NULL; i++) {
    struct mg_str caps[4] = {0};
    if (!mg_match(endpoint, mg_str(routes[i].pattern), caps)) continue;
    if (routes[i].rl_max &&
        rate_limit_check(&c->rem, routes[i].rl_max, routes[i].rl_window)) {
      ERROR_REPLY_429;
      return;
    }
    routes[i].handler(c, msg, caps, er, secret);
    return;
  }

  mg_http_reply(c, 404, JSON_HEADER, "{\"code\":404,\"error\":\"Not found\"}");
}

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *msg = (struct mg_http_message *)ev_data;
    struct mg_str caps[2];

    if (mg_match(msg->uri, mg_str("/api/#"), caps)) {
      const char *secret = getenv("JWT_SECRET");
      if (!secret) {
        mg_http_reply(c, 500, JSON_HEADER,
                      "{\"code\":500,\"error\":\"Internal Error\"}");
        fprintf(stderr, TERMINAL_ERROR_MESSAGE("JWT_SECRET not set"));
        exit(1);
      }

      const char *cors_origin = getenv("CORS_ORIGIN");
      if (!cors_origin) {
        fprintf(stderr, "CORS_ORIGIN not set, CORS disabled\n");
        cors_origin = "";
      }
      snprintf(g_json_header, sizeof(g_json_header),
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: %s\r\n"
               "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
               "Access-Control-Max-Age: 86400\r\n",
               cors_origin);

      if (mg_match(msg->method, mg_str("OPTIONS"), NULL)) {
        mg_http_reply(c, 204, g_json_header, "");
        return;
      }

      struct error_reply *er = NULL;
      dispatch(c, msg, caps[0], er, secret);
      return;
    }

    if (mg_match(msg->uri, mg_str("/uploads/#"), NULL)) {
      if (msg->uri.buf[msg->uri.len - 1] == '/') {
        mg_http_reply(c, 403, JSON_HEADER,
                      "{\"code\":403,\"error\":\"Forbidden\"}");
        return;
      }
      struct mg_http_serve_opts opts = {
          .root_dir = "/uploads=/var/www/uploads",
          .extra_headers = "Cache-Control: public, max-age=2592000\r\n"};
      mg_http_serve_dir(c, msg, &opts);
      return;
    }

    if (mg_match(msg->uri, mg_str("/docs#"), NULL)) {
      struct mg_http_serve_opts opts = {.root_dir = "/docs=./docs"};
      mg_http_serve_dir(c, msg, &opts);
      return;
    }

    mg_http_reply(c, 404, JSON_HEADER,
                  "{\"code\":404,\"error\":\"Not found\"}");
  } else if (ev == MG_EV_ERROR) {
    mg_http_reply(c, 500, JSON_HEADER,
                  "{\"code\":500,\"error\":\"Internal Error\"}");
  }
}

int main(void) {
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

  MagickWandGenesis();

  printf("===\tDB - opening...\t===\n");

  int atexit_rc = atexit(clean_db);
  if (atexit_rc != 0) {
    fprintf(stderr, "Couldn't register atexit handler. Error code: %d\n",
            atexit_rc);
    return EXIT_FAILURE;
  }

  signal(SIGTERM, handle_shutdown);
  signal(SIGINT, handle_shutdown);

  int db_rc = sqlite3_open("uwu.db", &db);
  if (SQLITE_OK != db_rc) {
    fprintf(stderr, "DB - Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return EXIT_FAILURE;
  }

  printf("===\tDB - successfully opened!\t===\n");

  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
#ifdef DEBUG
  mg_log_set(MG_LL_DEBUG);
#endif
  mg_http_listen(&mgr, "http://0.0.0.0:8000", ev_handler, NULL);

  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }

  return EXIT_SUCCESS;
}
