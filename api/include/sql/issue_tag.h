#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int issue_tag_exists(int issue_id, char *id);
int get_issue_tags_len(const struct mg_str *q, int issue_id);
int get_issue_tags(size_t len, struct issue_tag **arr, int issue_id, int page,
                   int page_size);
int add_issue_tag(struct issue_tag *issue_tag);
int delete_issue_tag(int issue_id, char *id);
