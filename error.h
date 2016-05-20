#ifndef ERROR_H
#define ERROR_H

typedef char TerrariaError;
TerrariaError *MALLOC_FAILED;

TerrariaError *_terraria_make_error(const char *message);
TerrariaError *_terraria_make_errorf(const char *format, ...);
TerrariaError *_terraria_make_perror(const char *message);

#endif // ERROR_H
