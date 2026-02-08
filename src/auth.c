#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <WinSock2.h>
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include "auth.h"
#include "db.h"
#include "json.h"
#include "password.h"

// ======================================================
// JSON helpers (strings)
// ======================================================
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

// ======================================================
// Token generator (32 bytes -> 64 hex chars)
// ======================================================
static int gen_token_hex(char *out, size_t out_size) {
  unsigned char bytes[32];
  if (out_size < 65) return 0;

  NTSTATUS st = BCryptGenRandom(NULL, bytes, (ULONG) sizeof(bytes),
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (st != 0) return 0;

  static const char *hex = "0123456789abcdef";
  for (size_t i = 0; i < sizeof(bytes); i++) {
    out[i * 2]     = hex[(bytes[i] >> 4) & 0xF];
    out[i * 2 + 1] = hex[bytes[i] & 0xF];
  }
  out[64] = '\0';
  return 1;
}

// ======================================================
// Bearer token extraction
// ======================================================
static int get_bearer_token(struct mg_http_message *hm, char *out, size_t out_size) {
  const struct mg_str *h = mg_http_get_header(hm, "Authorization");
  if (!h) return 0;

  const char *p = h->buf;
  int len = (int) h->len;

  const char *prefix = "Bearer ";
  int plen = (int) strlen(prefix);

  if (len <= plen) return 0;
  if (strncmp(p, prefix, (size_t) plen) != 0) return 0;

  int tlen = len - plen;
  if (tlen <= 0 || (size_t) tlen >= out_size) return 0;

  memcpy(out, p + plen, (size_t) tlen);
  out[tlen] = '\0';
  return 1;
}

// ======================================================
// Middleware: exige sessão válida e devolve user_id
// ======================================================
int auth_require_user(struct mg_connection *c, struct mg_http_message *hm, int *out_user_id) {
  char token[128];
  if (!get_bearer_token(hm, token, sizeof(token))) {
    mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing bearer token\" }\n");
    return 0;
  }

  const char *sql =
    "SELECT user_id "
    "FROM sessions "
    "WHERE token = ? AND (expires_at IS NULL OR expires_at > CURRENT_TIMESTAMP) "
    "LIMIT 1;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return 0;
  }

  sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid or expired session\" }\n");
    return 0;
  }

  int user_id = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);

  if (out_user_id) *out_user_id = user_id;
  return 1;
}

// ======================================================
// POST /login
// Body: { "email":"...", "password":"..." }
// Resposta: { "token":"...", "user": {...} }
// ======================================================
void handle_post_login(struct mg_connection *c, struct mg_http_message *hm) {
  if (hm->body.len == 0 || hm->body.len > 1024) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  char body[1025];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  char email[256], password[256];
  if (!json_get_string_field(body, "email", email, sizeof(email)) ||
      !json_get_string_field(body, "password", password, sizeof(password))) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing email/password\" }\n");
    return;
  }

  const char *sql =
    "SELECT id, password_hash, role, name, surname "
    "FROM users WHERE email = ? LIMIT 1;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, email, -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid credentials\" }\n");
    return;
  }

  // --- LER E COPIAR ANTES DO finalize() ---
  int user_id = sqlite3_column_int(stmt, 0);

  const unsigned char *hash_u    = sqlite3_column_text(stmt, 1);
  const unsigned char *role_u    = sqlite3_column_text(stmt, 2);
  const unsigned char *name_u    = sqlite3_column_text(stmt, 3);
  const unsigned char *surname_u = sqlite3_column_text(stmt, 4);

  char stored[512], role[32], name[128], surname[128];
  snprintf(stored, sizeof(stored), "%s", hash_u ? (const char *) hash_u : "");
  snprintf(role,   sizeof(role),   "%s", role_u ? (const char *) role_u : "");
  snprintf(name,   sizeof(name),   "%s", name_u ? (const char *) name_u : "");
  snprintf(surname,sizeof(surname),"%s", surname_u ? (const char *) surname_u : "");

  sqlite3_finalize(stmt);

  // Verificar password (PBKDF2) 
  int ok = 0;

  if (pwd_is_pbkdf2(stored)) {
    ok = pwd_verify(password, stored);
  } else {
    ok = (strcmp(stored, password) == 0);

    // Upgrade no 1º login bem sucedido
    if (ok) {
      char new_hash[256];
      if (pwd_hash(password, new_hash, sizeof(new_hash))) {
        sqlite3_stmt *up = NULL;
        const char *usql = "UPDATE users SET password_hash = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(db, usql, -1, &up, NULL) == SQLITE_OK && up) {
          sqlite3_bind_text(up, 1, new_hash, -1, SQLITE_TRANSIENT);
          sqlite3_bind_int(up, 2, user_id);
          sqlite3_step(up);
          sqlite3_finalize(up);
        }
      }
    }
  }

  if (!ok) {
    mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid credentials\" }\n");
    return;
  }

  // Criar sessão (7 dias)
  char token[65];
  if (!gen_token_hex(token, sizeof(token))) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"token generation failed\" }\n");
    return;
  }

  const char *ins =
    "INSERT INTO sessions (user_id, token, expires_at) "
    "VALUES (?, ?, datetime('now', '+7 days'));";

  rc = sqlite3_prepare_v2(db, ins, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_int(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, token, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"session insert failed\" }\n");
    return;
  }

  char esc_email[512], esc_role[64], esc_name[256], esc_surname[256];
  json_escape(email, esc_email, sizeof(esc_email));
  json_escape(role,  esc_role, sizeof(esc_role));
  json_escape(name,  esc_name, sizeof(esc_name));
  json_escape(surname, esc_surname, sizeof(esc_surname));

  mg_http_reply(c, 200, "Content-Type: application/json\r\n",
    "{ \"token\": \"%s\", \"user\": { \"id\": %d, \"email\": \"%s\", \"role\": \"%s\", \"name\": \"%s\", \"surname\": \"%s\" } }\n",
    token, user_id, esc_email, esc_role, esc_name, esc_surname);
}

// ======================================================
// POST /logout
// ======================================================
void handle_post_logout(struct mg_connection *c, struct mg_http_message *hm) {
  char token[128];
  if (!get_bearer_token(hm, token, sizeof(token))) {
    mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing bearer token\" }\n");
    return;
  }

  const char *sql = "DELETE FROM sessions WHERE token = ?;";
  sqlite3_stmt *stmt = NULL;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"logout failed\" }\n");
    return;
  }

  mg_http_reply(c, 204, "", "");
}

// ======================================================
// POST /signup (auto-login)
// Body: { "email":..., "password":..., "name":..., "surname":... }
// Resposta: { "token":"...", "user": {...} }
// ======================================================
void handle_post_signup(struct mg_connection *c, struct mg_http_message *hm) {
  if (hm->body.len == 0 || hm->body.len > 2048) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid body\" }\n");
    return;
  }

  char body[2049];
  memcpy(body, hm->body.buf, hm->body.len);
  body[hm->body.len] = '\0';

  char email[256], password[256], name[128], surname[128];
  if (!json_get_string_field(body, "email", email, sizeof(email)) ||
      !json_get_string_field(body, "password", password, sizeof(password)) ||
      !json_get_string_field(body, "name", name, sizeof(name)) ||
      !json_get_string_field(body, "surname", surname, sizeof(surname))) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"missing fields\" }\n");
    return;
  }

  if (strlen(email) < 3 || strlen(password) < 1 || strlen(name) < 1 || strlen(surname) < 1) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"invalid values\" }\n");
    return;
  }

  // Hash 
  char phash[256];
  if (!pwd_hash(password, phash, sizeof(phash))) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"hash failed\" }\n");
    return;
  }

  // 1) Criar user (client)
  const char *sql_user =
    "INSERT INTO users (email, password_hash, name, surname, role) "
    "VALUES (?, ?, ?, ?, 'client');";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql_user, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_text(stmt, 1, email, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, phash, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, surname, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 400, "Content-Type: application/json\r\n",
                  "{ \"error\": \"signup failed (email already exists?)\" }\n");
    return;
  }

  int user_id = (int) sqlite3_last_insert_rowid(db);

  // 2) Criar sessão (auto-login)
  char token[65];
  if (!gen_token_hex(token, sizeof(token))) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"token generation failed\" }\n");
    return;
  }

  const char *sql_sess =
    "INSERT INTO sessions (user_id, token, expires_at) "
    "VALUES (?, ?, datetime('now', '+7 days'));";

  rc = sqlite3_prepare_v2(db, sql_sess, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_int(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, token, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"session insert failed\" }\n");
    return;
  }

  // 3) Resposta
  char esc_email[512], esc_name[256], esc_surname[256];
  json_escape(email, esc_email, sizeof(esc_email));
  json_escape(name, esc_name, sizeof(esc_name));
  json_escape(surname, esc_surname, sizeof(esc_surname));

  mg_http_reply(c, 201, "Content-Type: application/json\r\n",
    "{ \"token\": \"%s\", \"user\": { \"id\": %d, \"email\": \"%s\", \"role\": \"client\", \"name\": \"%s\", \"surname\": \"%s\" } }\n",
    token, user_id, esc_email, esc_name, esc_surname);
}

// ======================================================
// GET /me
// ======================================================
void handle_get_me(struct mg_connection *c, struct mg_http_message *hm) {
  int user_id = 0;
  if (!auth_require_user(c, hm, &user_id)) return;

  const char *sql =
    "SELECT id, email, role, name, surname "
    "FROM users WHERE id = ? LIMIT 1;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                  "{ \"error\": \"db prepare failed\" }\n");
    return;
  }

  sqlite3_bind_int(stmt, 1, user_id);
  rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                  "{ \"error\": \"not found\" }\n");
    return;
  }

  const unsigned char *email_u   = sqlite3_column_text(stmt, 1);
  const unsigned char *role_u    = sqlite3_column_text(stmt, 2);
  const unsigned char *name_u    = sqlite3_column_text(stmt, 3);
  const unsigned char *surname_u = sqlite3_column_text(stmt, 4);

  char email[256], role[32], name[128], surname[128];
  snprintf(email, sizeof(email), "%s", email_u ? (const char *) email_u : "");
  snprintf(role,  sizeof(role),  "%s", role_u ? (const char *) role_u : "");
  snprintf(name,  sizeof(name),  "%s", name_u ? (const char *) name_u : "");
  snprintf(surname,sizeof(surname),"%s", surname_u ? (const char *) surname_u : "");

  sqlite3_finalize(stmt);

  char e1[512], r1[64], n1[256], s1[256];
  json_escape(email, e1, sizeof(e1));
  json_escape(role,  r1, sizeof(r1));
  json_escape(name,  n1, sizeof(n1));
  json_escape(surname, s1, sizeof(s1));

  mg_http_reply(c, 200, "Content-Type: application/json\r\n",
    "{ \"user\": { \"id\": %d, \"email\": \"%s\", \"role\": \"%s\", \"name\": \"%s\", \"surname\": \"%s\" } }\n",
    user_id, e1, r1, n1, s1);
}
