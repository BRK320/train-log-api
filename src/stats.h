#ifndef STATS_H
#define STATS_H

#include "mongoose.h"

// GET /stats/volume?days=N
void handle_get_stats_volume(struct mg_connection *c, struct mg_http_message *hm);

// GET /stats/prs
void handle_get_stats_prs(struct mg_connection *c, struct mg_http_message *hm);


#endif