#ifndef ADMIN_H
#define ADMIN_H

#include "mongoose.h"

// POST /admin/users
void handle_post_admin_users(struct mg_connection *c, struct mg_http_message *hm);
// GET /admin/users
void handle_get_admin_users(struct mg_connection *c);

// Middleware: exige sessão válida e role=admin
int auth_require_admin(struct mg_connection *c, struct mg_http_message *hm, int *out_user_id);

#endif
