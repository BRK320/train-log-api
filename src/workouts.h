#ifndef WORKOUTS_H
#define WORKOUTS_H

#include "mongoose.h"

// Workouts
void handle_get_workouts(struct mg_connection *c, struct mg_http_message *hm);
void handle_get_workouts_id(struct mg_connection *c, struct mg_http_message *hm);
void handle_post_workouts(struct mg_connection *c, struct mg_http_message *hm);
void handle_put_workouts(struct mg_connection *c, struct mg_http_message *hm);
void handle_delete_workouts(struct mg_connection *c, struct mg_http_message *hm);

// Sets
void handle_post_workout_set(struct mg_connection *c, struct mg_http_message *hm);
void handle_put_workout_set(struct mg_connection *c, struct mg_http_message *hm);
void handle_delete_workout_set(struct mg_connection *c, struct mg_http_message *hm);

#endif
