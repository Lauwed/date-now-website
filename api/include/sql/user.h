#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int user_exists(int id);
int user_identity_exists(char *username, char *email);
int get_users_len(const struct mg_str *q);
int get_users(size_t len, struct user **arr, const struct mg_str *q,
              const struct mg_str *sort, int page, int page_size);
int get_user(struct user *user, int id);
int get_user_by_email(struct user *user, char *email);
int get_user_totp_seed(char *email, char **seed);
int add_user(struct user *user);
int edit_user(struct user *user);
int delete_user(int id);
int get_subscriber_emails(size_t *len, char ***emails);
