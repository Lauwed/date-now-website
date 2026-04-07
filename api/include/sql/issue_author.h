
#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int issue_author_exists(int issue_id, int id);
int get_issue_authors_len(const struct mg_str *q, int issue_id);
int get_issue_authors(size_t len, struct issue_author **arr, int issue_id,
                      int page, int page_size);
int add_issue_author(struct issue_author *issue_author);
int delete_issue_author(int issue_id, int id);
