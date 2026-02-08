#include <stdio.h>
#include <string.h>
#include "exercises.h"
#include "db.h"
#include "json.h"

// ================= GET /exercises =================
void handle_get_exercises(struct mg_connection *c) {
  const char *sql = "SELECT id, name FROM exercises ORDER BY id;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  char json[8192];
  int pos = 0;
  pos += snprintf(json + pos, sizeof(json) - pos, "[");

  int first = 1;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *name_u = sqlite3_column_text(stmt, 1);
    const char *name = name_u ? (const char *)name_u : "";

    char esc[1024];
    if (!json_escape(name, esc, sizeof(esc))) esc[0] = '\0';

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "%s{ \"id\": %d, \"name\": \"%s\" }",
                    first ? "" : ",",
                    id, esc);
    first = 0;

    if (pos > (int)sizeof(json) - 200) break;
  }

  sqlite3_finalize(stmt);
  pos += snprintf(json + pos, sizeof(json) - pos, "]\n");

  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
}

// ================= GET /exercises/:id =================
void handle_get_exercises_id(struct mg_connection *c, struct mg_http_message *hm) {
  int id = -1;
  if (sscanf(hm->uri.buf, "/exercises/%d", &id) != 1 || id <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid id\" }\n");
    return;
  }

  const char *sql = "SELECT id, name FROM exercises WHERE id = ?;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_int(stmt, 1, id);
  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    int row_id = sqlite3_column_int(stmt, 0);
    const unsigned char *name_u = sqlite3_column_text(stmt, 1);
    const char *name = name_u ? (const char *)name_u : "";

    char esc[1024];
    if (!json_escape(name, esc, sizeof(esc))) esc[0] = '\0';

    mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                  "{ \"id\": %d, \"name\": \"%s\" }\n",
                  row_id, esc);
  } else if (rc == SQLITE_DONE) {
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{ \"error\": \"not found\" }\n");
  } else {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db step failed\" }\n");
  }

  sqlite3_finalize(stmt);
}

// ================= POST /exercises =================
void handle_post_exercises(struct mg_connection *c, struct mg_http_message *hm) {
  if (hm->body.len == 0 || hm->body.len > 512) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  char body[513];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  char name[256];
  if (!json_get_name(body, name, sizeof(name))) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid json\" }\n");
    return;
  }

  const char *sql = "INSERT INTO exercises(name) VALUES (?);";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"insert failed\" }\n");
    return;
  }

  sqlite3_int64 id = sqlite3_last_insert_rowid(db);

  char esc[1024];
  if (!json_escape(name, esc, sizeof(esc))) esc[0] = '\0';

  mg_http_reply(c, 201, "Content-Type: application/json\r\n",
                "{ \"id\": %lld, \"name\": \"%s\" }\n",
                (long long)id, esc);
}

// ================= PUT /exercises/:id =================
void handle_put_exercises(struct mg_connection *c,
                          struct mg_http_message *hm) {
  if (hm->body.len == 0 || hm->body.len > 512) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  int id = -1;
  if (sscanf(hm->uri.buf, "/exercises/%d", &id) != 1 || id <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid id\" }\n");
    return;
  }

  char body[513];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  char name[256];
  if (!json_get_name(body, name, sizeof(name))) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid json\" }\n");
    return;
  }

  const char *sql = "UPDATE exercises SET name = ? WHERE id = ?;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, id);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"update failed\" }\n");
    return;
  }

  if (sqlite3_changes(db) == 0) {
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{ \"error\": \"not found\" }\n");
    return;
  }

  char esc[1024];
  if (!json_escape(name, esc, sizeof(esc))) esc[0] = '\0';

  mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{ \"id\": %d, \"name\": \"%s\" }\n",
                id, esc);
}

// ================= DELETE /exercises/:id =================
void handle_delete_exercises(struct mg_connection *c,
                             struct mg_http_message *hm) {
  int id = -1;
  if (sscanf(hm->uri.buf, "/exercises/%d", &id) != 1 || id <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid id\" }\n");
    return;
  }

  const char *sql = "DELETE FROM exercises WHERE id = ?;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_int(stmt, 1, id);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"delete failed\" }\n");
    return;
  }

  if (sqlite3_changes(db) == 0) {
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{ \"error\": \"not found\" }\n");
    return;
  }

  mg_http_reply(c, 204, "", "");
}
