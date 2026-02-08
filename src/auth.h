#ifndef AUTH_H
#define AUTH_H

#include "mongoose.h"

// POST /login
void handle_post_login(struct mg_connection *c, struct mg_http_message *hm);

// Lê "Authorization: Bearer <token>" e devolve user_id se a sessão for válida
// Retorna 1 se ok, 0 se falhou (sem reply, o handler deve responder com 401)
int auth_require_user(struct mg_connection *c, struct mg_http_message *hm, int *out_user_id);

// Middleware: exige sessão válida e devolve user_id, só para admins
int auth_require_admin(struct mg_connection *c, struct mg_http_message *hm, int *out_user_id);

void handle_post_logout(struct mg_connection *c, struct mg_http_message *hm);

// POST /signup (criar conta client)
void handle_post_signup(struct mg_connection *c, struct mg_http_message *hm);

// GET /me (ver user logado)
void handle_get_me(struct mg_connection *c, struct mg_http_message *hm);

#endif
