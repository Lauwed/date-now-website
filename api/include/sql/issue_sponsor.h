

#pragma once

#include <lib/mongoose.h>
#include <stddef.h>
#include <structs.h>

int issue_sponsor_exists(int issue_id, char *id);
int get_issue_sponsors_len(const struct mg_str *q, int issue_id);
int get_issue_sponsors(size_t len, struct issue_sponsor **arr, int issue_id,
                       int page, int page_size);
int add_issue_sponsor(struct issue_sponsor *issue_sponsor);
int delete_issue_sponsor(int issue_id, char *id);
