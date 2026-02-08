#include <stdio.h>
#include <string.h>

#include "workouts.h"
#include "db.h"
#include "json.h"
#include "auth.h"

// ------------------ Helpers JSON parse ------------------
static int json_get_int_field(const char *json, const char *field, int *out) {
  char key[64];
  if (snprintf(key, sizeof(key), "\"%s\"", field) <= 0) return 0;

  const char *p = strstr(json, key);
  if (!p) return 0;
  p += strlen(key);

  while (*p && *p != ':') p++;
  if (*p != ':') return 0;
  p++;

  while (*p == ' ' || *p == '\t') p++;

  int v = 0;
  if (sscanf(p, "%d", &v) != 1) return 0;
  *out = v;
  return 1;
}

static int json_get_double_field(const char *json, const char *field, double *out) {
  char key[64];
  if (snprintf(key, sizeof(key), "\"%s\"", field) <= 0) return 0;

  const char *p = strstr(json, key);
  if (!p) return 0;
  p += strlen(key);

  while (*p && *p != ':') p++;
  if (*p != ':') return 0;
  p++;

  while (*p == ' ' || *p == '\t') p++;

  double v = 0;
  if (sscanf(p, "%lf", &v) != 1) return 0;
  *out = v;
  return 1;
}

static int parse_id_from_uri(const char *uri, const char *fmt, int *out) {
  int v = -1;
  if (sscanf(uri, fmt, &v) != 1 || v <= 0) return 0;
  *out = v;
  return 1;
}

static void reply_db_prepare_failed(struct mg_connection *c) {
  mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                "{ \"error\": \"db prepare failed\" }\n");
}

// ------------------ GET /workouts ------------------
void handle_get_workouts(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  const char *sql =
    "SELECT id, created_at "
    "FROM workouts "
    "WHERE user_id = ? "
    "ORDER BY id DESC;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt, 1, user_id);

  char json[8192];
  int pos = 0;
  pos += snprintf(json + pos, (int)sizeof(json) - pos, "[");

  int first = 1;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *dt_u = sqlite3_column_text(stmt, 1);
    const char *dt = dt_u ? (const char *)dt_u : "";

    char esc_dt[128];
    if (!json_escape(dt, esc_dt, sizeof(esc_dt))) esc_dt[0] = '\0';

    pos += snprintf(json + pos, (int)sizeof(json) - pos,
                    "%s{ \"id\": %d, \"created_at\": \"%s\" }",
                    first ? "" : ",",
                    id, esc_dt);
    first = 0;

    if (pos > (int)sizeof(json) - 200) break;
  }

  sqlite3_finalize(stmt);

  pos += snprintf(json + pos, (int)sizeof(json) - pos, "]\n");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
}

// ------------------ GET /workouts/:id ------------------
void handle_get_workouts_id(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  int workout_id = -1;
  if (!parse_id_from_uri(hm->uri.buf, "/workouts/%d", &workout_id)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid id\" }\n");
    return;
  }

  // 1) confirmar que o workout é do user
  const char *sql_w =
    "SELECT id, created_at "
    "FROM workouts "
    "WHERE id = ? AND user_id = ? "
    "LIMIT 1;";

  sqlite3_stmt *stmt_w = NULL;
  int rc = sqlite3_prepare_v2(db, sql_w, -1, &stmt_w, NULL);
  if (rc != SQLITE_OK || !stmt_w) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt_w, 1, workout_id);
  sqlite3_bind_int(stmt_w, 2, user_id);

  rc = sqlite3_step(stmt_w);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt_w);
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{ \"error\": \"not found\" }\n");
    return;
  }

  const unsigned char *dt_u = sqlite3_column_text(stmt_w, 1);
  const char *dt = dt_u ? (const char *)dt_u : "";
  char esc_dt[128];
  if (!json_escape(dt, esc_dt, sizeof(esc_dt))) esc_dt[0] = '\0';

  sqlite3_finalize(stmt_w);

  // 2) listar sets desse workout (com nome do exercício)
  const char *sql_s =
    "SELECT we.id, we.exercise_id, e.name, we.reps, we.weight "
    "FROM workout_exercises we "
    "JOIN exercises e ON e.id = we.exercise_id "
    "WHERE we.workout_id = ? "
    "ORDER BY we.id;";

  sqlite3_stmt *stmt_s = NULL;
  rc = sqlite3_prepare_v2(db, sql_s, -1, &stmt_s, NULL);
  if (rc != SQLITE_OK || !stmt_s) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt_s, 1, workout_id);

  char json[16384];
  int pos = 0;

  pos += snprintf(json + pos, (int)sizeof(json) - pos,
                  "{ \"id\": %d, \"created_at\": \"%s\", \"sets\": [",
                  workout_id, esc_dt);

  int first = 1;
  while ((rc = sqlite3_step(stmt_s)) == SQLITE_ROW) {
    int set_id = sqlite3_column_int(stmt_s, 0);
    int ex_id  = sqlite3_column_int(stmt_s, 1);
    const unsigned char *name_u = sqlite3_column_text(stmt_s, 2);
    const char *name = name_u ? (const char *)name_u : "";
    int reps = sqlite3_column_int(stmt_s, 3);
    double weight = sqlite3_column_double(stmt_s, 4);

    char esc_name[1024];
    if (!json_escape(name, esc_name, sizeof(esc_name))) esc_name[0] = '\0';

    pos += snprintf(json + pos, (int)sizeof(json) - pos,
      "%s{ \"id\": %d, \"exercise_id\": %d, \"exercise_name\": \"%s\", \"reps\": %d, \"weight\": %.3f }",
      first ? "" : ",",
      set_id, ex_id, esc_name, reps, weight);

    first = 0;

    if (pos > (int)sizeof(json) - 400) break;
  }

  sqlite3_finalize(stmt_s);

  pos += snprintf(json + pos, (int)sizeof(json) - pos, "] }\n");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
}

// ------------------ POST /workouts ------------------
void handle_post_workouts(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  const char *sql = "INSERT INTO workouts(user_id) VALUES (?);";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt, 1, user_id);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"insert failed\" }\n");
    return;
  }

  int id = (int) sqlite3_last_insert_rowid(db);
  mg_http_reply(c, 201, "Content-Type: application/json\r\n",
                "{ \"id\": %d }\n", id);
}

// ------------------ PUT /workouts/:id ------------------
// Por agora devolve "not implemented"
void handle_put_workouts(struct mg_connection *c, struct mg_http_message *hm) {
  (void) hm;
  mg_http_reply(c, 501, "Content-Type: application/json\r\n",
                "{ \"error\": \"not implemented\" }\n");
}

// ------------------ DELETE /workouts/:id ------------------
void handle_delete_workouts(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  int workout_id = -1;
  if (!parse_id_from_uri(hm->uri.buf, "/workouts/%d", &workout_id)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid id\" }\n");
    return;
  }

  // Apagar workout só se for do user
  const char *sql =
    "DELETE FROM workouts WHERE id = ? AND user_id = ?;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt, 1, workout_id);
  sqlite3_bind_int(stmt, 2, user_id);

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

// ------------------ POST /workouts/:id/sets ------------------
void handle_post_workout_set(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  int workout_id = -1;
  if (sscanf(hm->uri.buf, "/workouts/%d/sets", &workout_id) != 1 || workout_id <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid workout id\" }\n");
    return;
  }

  // Confirmar que o workout é do user
  {
    const char *sql = "SELECT 1 FROM workouts WHERE id = ? AND user_id = ? LIMIT 1;";
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &s, NULL);
    if (rc != SQLITE_OK || !s) { reply_db_prepare_failed(c); return; }
    sqlite3_bind_int(s, 1, workout_id);
    sqlite3_bind_int(s, 2, user_id);
    rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_ROW) {
      mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                    "{ \"error\": \"workout not found\" }\n");
      return;
    }
  }

  if (hm->body.len == 0 || hm->body.len > 512) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  char body[513];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  int exercise_id = 0, reps = 0;
  double weight = 0.0;

  if (!json_get_int_field(body, "exercise_id", &exercise_id) ||
      !json_get_int_field(body, "reps", &reps) ||
      !json_get_double_field(body, "weight", &weight)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing fields\" }\n");
    return;
  }

  if (exercise_id <= 0 || reps <= 0 || weight <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid values\" }\n");
    return;
  }

  // Confirmar que exercise existe
  {
    const char *sql = "SELECT 1 FROM exercises WHERE id = ? LIMIT 1;";
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &s, NULL);
    if (rc != SQLITE_OK || !s) { reply_db_prepare_failed(c); return; }
    sqlite3_bind_int(s, 1, exercise_id);
    rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_ROW) {
      mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                    "{ \"error\": \"exercise not found\" }\n");
      return;
    }
  }

  const char *sql =
    "INSERT INTO workout_exercises(workout_id, exercise_id, reps, weight) "
    "VALUES (?, ?, ?, ?);";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt, 1, workout_id);
  sqlite3_bind_int(stmt, 2, exercise_id);
  sqlite3_bind_int(stmt, 3, reps);
  sqlite3_bind_double(stmt, 4, weight);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"insert failed\" }\n");
    return;
  }

  int set_id = (int) sqlite3_last_insert_rowid(db);

  mg_http_reply(c, 201, "Content-Type: application/json\r\n",
                "{ \"id\": %d, \"workout_id\": %d, \"exercise_id\": %d, \"reps\": %d, \"weight\": %.3f }\n",
                set_id, workout_id, exercise_id, reps, weight);
}

// ------------------ PUT /workouts/:id/sets/:set_id ------------------
void handle_put_workout_set(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  int workout_id = -1, set_id = -1;
  if (sscanf(hm->uri.buf, "/workouts/%d/sets/%d", &workout_id, &set_id) != 2 ||
      workout_id <= 0 || set_id <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid ids\" }\n");
    return;
  }

  // Confirmar que o workout é do user
  {
    const char *sql = "SELECT 1 FROM workouts WHERE id = ? AND user_id = ? LIMIT 1;";
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &s, NULL);
    if (rc != SQLITE_OK || !s) { reply_db_prepare_failed(c); return; }
    sqlite3_bind_int(s, 1, workout_id);
    sqlite3_bind_int(s, 2, user_id);
    rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_ROW) {
      mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                    "{ \"error\": \"workout not found\" }\n");
      return;
    }
  }

  if (hm->body.len == 0 || hm->body.len > 512) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  char body[513];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  int reps = 0;
  double weight = 0.0;

  if (!json_get_int_field(body, "reps", &reps) ||
      !json_get_double_field(body, "weight", &weight)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing fields\" }\n");
    return;
  }

  if (reps <= 0 || weight <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid values\" }\n");
    return;
  }

  const char *sql =
    "UPDATE workout_exercises "
    "SET reps = ?, weight = ? "
    "WHERE id = ? AND workout_id = ?;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt, 1, reps);
  sqlite3_bind_double(stmt, 2, weight);
  sqlite3_bind_int(stmt, 3, set_id);
  sqlite3_bind_int(stmt, 4, workout_id);

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

  mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{ \"id\": %d, \"workout_id\": %d, \"reps\": %d, \"weight\": %.3f }\n",
                set_id, workout_id, reps, weight);
}

// ------------------ DELETE /workouts/:id/sets/:set_id ------------------
void handle_delete_workout_set(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  int workout_id = -1, set_id = -1;
  if (sscanf(hm->uri.buf, "/workouts/%d/sets/%d", &workout_id, &set_id) != 2 ||
      workout_id <= 0 || set_id <= 0) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid ids\" }\n");
    return;
  }

  // Confirmar que o workout é do user
  {
    const char *sql = "SELECT 1 FROM workouts WHERE id = ? AND user_id = ? LIMIT 1;";
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &s, NULL);
    if (rc != SQLITE_OK || !s) { reply_db_prepare_failed(c); return; }
    sqlite3_bind_int(s, 1, workout_id);
    sqlite3_bind_int(s, 2, user_id);
    rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_ROW) {
      mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                    "{ \"error\": \"workout not found\" }\n");
      return;
    }
  }

  const char *sql =
    "DELETE FROM workout_exercises "
    "WHERE id = ? AND workout_id = ?;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) { reply_db_prepare_failed(c); return; }

  sqlite3_bind_int(stmt, 1, set_id);
  sqlite3_bind_int(stmt, 2, workout_id);

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
