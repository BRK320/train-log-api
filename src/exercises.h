#ifndef EXERCISES_H
#define EXERCISES_H

#include "mongoose.h"

// GET /exercises
void handle_get_exercises(struct mg_connection *c);

// GET /exercises/:id
void handle_get_exercises_id(struct mg_connection *c,
                             struct mg_http_message *hm);

// POST /exercises
void handle_post_exercises(struct mg_connection *c,
                           struct mg_http_message *hm);

// PUT /exercises/:id
void handle_put_exercises(struct mg_connection *c,
                          struct mg_http_message *hm);

// DELETE /exercises/:id
void handle_delete_exercises(struct mg_connection *c,
                             struct mg_http_message *hm);

#endif
