#ifndef ERROR_H
#define ERROR_H

typedef char* TerrariaError;
TerrariaError *MALLOC_FAILED;

TerrariaError *_terraria_make_error(const char *message);
TerrariaError *_terraria_make_errorf(const char *format, ...);
TerrariaError *_terraria_make_perror(const char *message);
TerrariaError *_terraria_make_strerror();

void terraria_free_error(TerrariaError *error);

#endif // ERROR_H
