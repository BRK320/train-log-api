#ifndef JSON_H
#define JSON_H

#include <stddef.h>

// Extrai e faz unescape do "name" num JSON simples: { "name": "..." }
// Suporta escapes: \" \\ \n \r \t \b \f
// Retorna 1 se sucesso, 0 se erro.
int json_get_name(const char *json, char *out, size_t out_size);

// Escapa uma string para ser segura dentro de "..." em JSON
// Retorna 1 se coube em out, 0 se não coube.
int json_escape(const char *in, char *out, size_t out_size);

#endif
