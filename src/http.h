#ifndef HTTP_H
#define HTTP_H

#include "mongoose.h"

// Helpers de método HTTP
int is_get(struct mg_http_message *hm);
int is_post(struct mg_http_message *hm);
int is_put(struct mg_http_message *hm);
int is_delete(struct mg_http_message *hm);

// Handlers genéricos
void handle_health(struct mg_connection *c);
void handle_not_found(struct mg_connection *c);

// Router principal
void ev_handler(struct mg_connection *c, int ev, void *ev_data);

static int serve_static(struct mg_connection *c, struct mg_http_message *hm);

#endif
