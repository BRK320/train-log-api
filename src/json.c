#include <string.h>
#include <stdio.h>
#include "json.h"

// Extrai e faz unescape do "name" num JSON simples
int json_get_name(const char *json, char *out, size_t out_size) {
  const char *key = "\"name\"";
  const char *p = strstr(json, key);
  if (!p) return 0;

  p += strlen(key);
  while (*p && *p != ':') p++;
  if (*p != ':') return 0;
  p++;

  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return 0;
  p++; // dentro da string

  size_t o = 0;
  while (*p && o < out_size - 1) {
    if (*p == '"') {  // fim
      out[o] = '\0';
      return 1;
    }

    if (*p == '\\') { // escape
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
        default:
          return 0; // não suporta \uXXXX
      }
      out[o++] = ch;
      p++;
    } else {
      out[o++] = *p++;
    }
  }

  return 0;
}

// Escapa string para JSON
int json_escape(const char *in, char *out, size_t out_size) {
  size_t o = 0;

  for (size_t i = 0; in[i] != '\0'; i++) {
    unsigned char ch = (unsigned char)in[i];
    const char *rep = NULL;
    char tmp[7];

    switch (ch) {
      case '\"': rep = "\\\""; break;
      case '\\': rep = "\\\\"; break;
      case '\b': rep = "\\b";  break;
      case '\f': rep = "\\f";  break;
      case '\n': rep = "\\n";  break;
      case '\r': rep = "\\r";  break;
      case '\t': rep = "\\t";  break;
      default:
        if (ch < 0x20) {
          snprintf(tmp, sizeof(tmp), "\\u%04X", ch);
          rep = tmp;
        }
        break;
    }

    if (rep) {
      size_t rlen = strlen(rep);
      if (o + rlen >= out_size) return 0;
      memcpy(out + o, rep, rlen);
      o += rlen;
    } else {
      if (o + 1 >= out_size) return 0;
      out[o++] = (char)ch;
    }
  }

  if (o >= out_size) return 0;
  out[o] = '\0';
  return 1;
}
