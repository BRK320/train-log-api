#include <stdio.h>
#include "mongoose.h"
#include "db.h"
#include "http.h"

int main(void) {
  struct mg_mgr mgr;

  db_init();
  if (!db) return 1;

  mg_mgr_init(&mgr);
  printf("Listening on http://localhost:8000\n");
  mg_http_listen(&mgr, "http://0.0.0.0:8000", ev_handler, NULL);

  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }

  db_close();
  mg_mgr_free(&mgr);
  return 0;
}
