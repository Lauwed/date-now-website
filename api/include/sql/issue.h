#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int issue_exists(int id);
int issue_identity_exists(char *title, int issue_number, char *slug);
int get_issues_len(const struct mg_str *q);
int get_issues(size_t len, struct issue **arr, const struct mg_str *q,
               const struct mg_str *sort, int page, int page_size);
int get_issue(struct issue *issue, int id);
int add_issue(struct issue *issue);
int edit_issue(struct issue *issue);
int delete_issue(int id);
int publish_issue(int id);
