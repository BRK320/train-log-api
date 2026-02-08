#include <stdio.h>
#include <string.h>

#include "admin.h"
#include "auth.h"
#include "db.h"
#include "json.h"
#include "password.h"

// Parse simples de string field: "email", "password", "name", "surname", "role"
static int json_get_string_field(const char *json,
                                 const char *field_name,
                                 char *out, size_t out_size) {
  char key[64];
  if (snprintf(key, sizeof(key), "\"%s\"", field_name) <= 0) return 0;

  const char *p = strstr(json, key);
  if (!p) return 0;

  p += strlen(key);
  while (*p && *p != ':') p++;
  if (*p != ':') return 0;
  p++;

  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return 0;
  p++;

  size_t o = 0;
  while (*p && o < out_size - 1) {
    if (*p == '"') { out[o] = '\0'; return 1; }

    if (*p == '\\') {
      p++;
      if (!*p) return 0;

      char ch;
      switch (*p) {
        case '"':  ch = '"';  break;
        case '\\': ch = '\\'; break;
        case 'n':  ch = '\n'; break;
        case 'r':  ch = '\r'; break;
        case 't':  ch = '\t'; break;
        case 'b':  ch = '\b'; break;
        case 'f':  ch = '\f'; break;
        default: return 0;
      }

      out[o++] = ch;
      p++;
    } else {
      out[o++] = *p++;
    }
  }

  return 0;
}

static int role_valid(const char *role) {
  return (strcmp(role, "admin") == 0) || (strcmp(role, "client") == 0);
}

// ======================================================
// Middleware: exige sessão + admin
// ======================================================
int auth_require_admin(struct mg_connection *c, struct mg_http_message *hm, int *out_user_id) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return 0;

  const char *sql = "SELECT role FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return 0;
  }

  sqlite3_bind_int(stmt, 1, user_id);
  rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid session\" }\n");
    return 0;
  }

  const unsigned char *role_u = sqlite3_column_text(stmt, 0);

  // COPIAR antes do finalize
  char role[32];
  snprintf(role, sizeof(role), "%s", role_u ? (const char *) role_u : "");

  sqlite3_finalize(stmt);

  if (strcmp(role, "admin") != 0) {
    mg_http_reply(c, 403, "Content-Type: application/json\r\n",
                  "{ \"error\": \"admin only\" }\n");
    return 0;
  }

  if (out_user_id) *out_user_id = user_id;
  return 1;
}

// ======================================================
// POST /admin/users
// Body: { "email": "...", "password":"...", "name":"...", "surname":"...", "role":"admin|client" }
// ======================================================
void handle_post_admin_users(struct mg_connection *c, struct mg_http_message *hm) {
  if (hm->body.len == 0 || hm->body.len > 1024) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  char body[1025];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  char email[256], password[256], name[128], surname[128], role[32];

  if (!json_get_string_field(body, "email", email, sizeof(email)) ||
      !json_get_string_field(body, "password", password, sizeof(password)) ||
      !json_get_string_field(body, "name", name, sizeof(name)) ||
      !json_get_string_field(body, "surname", surname, sizeof(surname)) ||
      !json_get_string_field(body, "role", role, sizeof(role))) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing fields\" }\n");
    return;
  }

  if (!role_valid(role)) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid role\" }\n");
    return;
  }

  // Hash PBKDF2
  char phash[256];
  if (!pwd_hash(password, phash, sizeof(phash))) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"hash failed\" }\n");
    return;
  }

  const char *sql =
    "INSERT INTO users (email, password_hash, name, surname, role) "
    "VALUES (?, ?, ?, ?, ?);";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, email, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, phash, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, surname, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, role, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"insert failed (email already exists?)\" }\n");
    return;
  }

  sqlite3_int64 id = sqlite3_last_insert_rowid(db);

  char esc_email[512], esc_name[256], esc_surname[256], esc_role[64];
  json_escape(email, esc_email, sizeof(esc_email));
  json_escape(name, esc_name, sizeof(esc_name));
  json_escape(surname, esc_surname, sizeof(esc_surname));
  json_escape(role, esc_role, sizeof(esc_role));

  mg_http_reply(c, 201, "Content-Type: application/json\r\n",
                "{ \"id\": %lld, \"email\": \"%s\", \"name\": \"%s\", \"surname\": \"%s\", \"role\": \"%s\" }\n",
                (long long) id, esc_email, esc_name, esc_surname, esc_role);
}

// ======================================================
// GET /admin/users  
// ======================================================
void handle_get_admin_users(struct mg_connection *c) {
  const char *sql = "SELECT id, email, role, name, surname FROM users ORDER BY id;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  char json[8192];
  int pos = 0;
  pos += snprintf(json + pos, (int)sizeof(json) - pos, "[");

  int first = 1;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);

    const unsigned char *email_u   = sqlite3_column_text(stmt, 1);
    const unsigned char *role_u    = sqlite3_column_text(stmt, 2);
    const unsigned char *name_u    = sqlite3_column_text(stmt, 3);
    const unsigned char *surname_u = sqlite3_column_text(stmt, 4);

    const char *email   = email_u ? (const char *)email_u : "";
    const char *role    = role_u ? (const char *)role_u : "";
    const char *name    = name_u ? (const char *)name_u : "";
    const char *surname = surname_u ? (const char *)surname_u : "";

    char e1[512], r1[64], n1[256], s1[256];
    json_escape(email, e1, sizeof(e1));
    json_escape(role, r1, sizeof(r1));
    json_escape(name, n1, sizeof(n1));
    json_escape(surname, s1, sizeof(s1));

    pos += snprintf(json + pos, (int)sizeof(json) - pos,
                    "%s{ \"id\": %d, \"email\": \"%s\", \"role\": \"%s\", \"name\": \"%s\", \"surname\": \"%s\" }",
                    first ? "" : ",", id, e1, r1, n1, s1);
    first = 0;

    if (pos > (int)sizeof(json) - 256) break;
  }

  sqlite3_finalize(stmt);

  pos += snprintf(json + pos, (int)sizeof(json) - pos, "]\n");
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json);
}
