#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stats.h"
#include "db.h"
#include "json.h"

void handle_get_stats_volume(struct mg_connection *c, struct mg_http_message *hm) {
  char days_buf[16];
  int days = 7;

  if (mg_http_get_var(&hm->query, "days", days_buf, sizeof(days_buf)) > 0) {
    int d = atoi(days_buf);
    if (d > 0 && d <= 3650) days = d; 
  }

  char modifier[32];
  snprintf(modifier, sizeof(modifier), "-%d days", days);

  const char *sql =
    "SELECT date(w.created_at) AS day, "
    "       COALESCE(SUM(we.reps * we.weight), 0) AS volume "
    "FROM workouts w "
    "LEFT JOIN workout_exercises we ON we.workout_id = w.id "
    "WHERE w.created_at >= datetime('now', ?) "
    "GROUP BY day "
    "ORDER BY day;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, modifier, -1, SQLITE_TRANSIENT);

  char json[8192];
  int pos = 0;
  pos += snprintf(json + pos, sizeof(json) - pos, "[");

  int first = 1;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const unsigned char *day_u = sqlite3_column_text(stmt, 0);
    const char *day = day_u ? (const char *)day_u : "";
    double volume = sqlite3_column_double(stmt, 1);

    char esc_day[64];
    if (!json_escape(day, esc_day, sizeof(esc_day))) esc_day[0] = '\0';

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "%s{ \"day\": \"%s\", \"volume\": %.3f }",
                    first ? "" : ",",
                    esc_day, volume);
    first = 0;

    if (pos > (int)sizeof(json) - 200) break;
  }

  sqlite3_finalize(stmt);
  pos += snprintf(json + pos, sizeof(json) - pos, "]\n");

  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
}

void handle_get_stats_prs(struct mg_connection *c, struct mg_http_message *hm) {
  (void) hm;

  // PRs por exercício:
  // - max_weight: maior peso levantado em qualquer set
  // - max_reps: maior reps em qualquer set
  // - max_volume: maior (reps*weight) num set
  const char *sql =
    "SELECT "
    "  e.id, e.name, "
    "  COALESCE(MAX(we.weight), 0) AS max_weight, "
    "  COALESCE(MAX(we.reps), 0) AS max_reps, "
    "  COALESCE(MAX(we.reps * we.weight), 0) AS max_volume "
    "FROM exercises e "
    "LEFT JOIN workout_exercises we ON we.exercise_id = e.id "
    "GROUP BY e.id, e.name "
    "ORDER BY e.id;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || stmt == NULL) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  char json[16384];
  int pos = 0;
  pos += snprintf(json + pos, sizeof(json) - pos, "[");

  int first = 1;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int ex_id = sqlite3_column_int(stmt, 0);
    const unsigned char *name_u = sqlite3_column_text(stmt, 1);
    const char *name = name_u ? (const char *)name_u : "";

    double max_weight = sqlite3_column_double(stmt, 2);
    int max_reps = sqlite3_column_int(stmt, 3);
    double max_volume = sqlite3_column_double(stmt, 4);

    char esc_name[1024];
    if (!json_escape(name, esc_name, sizeof(esc_name))) esc_name[0] = '\0';

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "%s{ \"exercise_id\": %d, \"exercise_name\": \"%s\", "
                    "\"max_weight\": %.3f, \"max_reps\": %d, \"max_volume\": %.3f }",
                    first ? "" : ",",
                    ex_id, esc_name, max_weight, max_reps, max_volume);
    first = 0;

    if (pos > (int)sizeof(json) - 400) break;
  }

  sqlite3_finalize(stmt);

  pos += snprintf(json + pos, sizeof(json) - pos, "]\n");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
}
