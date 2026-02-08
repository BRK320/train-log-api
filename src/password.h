#ifndef PASSWORD_H
#define PASSWORD_H

#include <stddef.h>

int pwd_hash(const char *password, char *out, size_t out_size);
int pwd_verify(const char *password, const char *stored);

int pwd_is_pbkdf2(const char *stored);

#endif
