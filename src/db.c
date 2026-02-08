#include <stdio.h>
#include <string.h>
#include "db.h"

// Handle global da base de dados
sqlite3 *db = NULL;

// ======================================================
// Seed admin (INTERNO ao db.c)
// ======================================================
static void db_seed_admin(void) {
  const char *check_sql =
    "SELECT 1 FROM users WHERE role = 'admin' LIMIT 1;";

  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    printf("Erro ao verificar admin\n");
    return;
  }

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Já existe admin → não faz nada
  if (rc == SQLITE_ROW) {
    return;
  }

  printf("Nenhum admin encontrado. A criar admin default...\n");

  const char *insert_sql =
    "INSERT INTO users (email, password_hash, name, surname, role) "
    "VALUES (?, ?, ?, ?, 'admin');";

  rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK || !stmt) {
    printf("Erro ao preparar insert admin\n");
    return;
  }

  // Password inicial simples (é re-hash no 1º login)
  sqlite3_bind_text(stmt, 1, "admin@local", -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, "admin", -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, "Admin", -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, "System", -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc == SQLITE_DONE) {
    printf("Admin default criado: admin@local / admin\n");
  } else {
    printf("Erro ao criar admin default\n");
  }
}

// ======================================================
// Init DB
// ======================================================
void db_init(void) {
  int rc;
  char *err = NULL;

  rc = sqlite3_open("db/gym.db", &db);
  if (rc != SQLITE_OK) {
    printf("Erro ao abrir BD: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    db = NULL;
    return;
  }

  const char *sql =
    "CREATE TABLE IF NOT EXISTS exercises ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS workouts ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS workout_exercises ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  workout_id INTEGER NOT NULL,"
    "  exercise_id INTEGER NOT NULL,"
    "  reps INTEGER NOT NULL,"
    "  weight REAL NOT NULL,"
    "  FOREIGN KEY(workout_id) REFERENCES workouts(id),"
    "  FOREIGN KEY(exercise_id) REFERENCES exercises(id)"
    ");"

    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  email TEXT NOT NULL UNIQUE,"
    "  password_hash TEXT NOT NULL,"
    "  name TEXT NOT NULL,"
    "  surname TEXT NOT NULL,"
    "  role TEXT NOT NULL CHECK(role IN ('admin','client')),"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER NOT NULL,"
    "  token TEXT NOT NULL UNIQUE,"
    "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "  expires_at DATETIME,"
    "  FOREIGN KEY(user_id) REFERENCES users(id)"
    ");";

  rc = sqlite3_exec(db, sql, NULL, NULL, &err);
  if (rc != SQLITE_OK) {
    printf("Erro SQL: %s\n", err);
    sqlite3_free(err);
  } else {
    printf("BD pronta.\n");
    printf("DEBUG: a correr seed admin...\n");
    db_seed_admin();
    printf("DEBUG: seed admin feito.\n");
  }
}

// ======================================================
// Close DB
// ======================================================
void db_close(void) {
  if (db) {
    sqlite3_close(db);
    db = NULL;
  }
}
