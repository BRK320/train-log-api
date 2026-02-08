#ifndef DB_H
#define DB_H

#include "./libsqlite3/sqlite3.h"

// Handle global da base de dados
extern sqlite3 *db;

// Inicializa a base de dados (abre ficheiro e cria tabelas)
void db_init(void);

// Fecha a base de dados
void db_close(void);

#endif
