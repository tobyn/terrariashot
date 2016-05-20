#include <malloc.h>
#include <memory.h>
#include <errno.h>
#include <stdarg.h>

#include "error.h"

#define TERRARIA_ERROR_DEFAULT_BUFFER_SIZE 1024

TerrariaError *MALLOC_FAILED = (TerrariaError *) "Memory allocation failed";

TerrariaError *_terraria_make_error(const char *message) {
    size_t message_length = strlen(message);
    TerrariaError *error = malloc(message_length);
    if (error == NULL)
        return MALLOC_FAILED;

    memcpy(error, message, message_length);

    return error;
}

TerrariaError *_terraria_make_errorf(const char *format, ...) {
    char *error = malloc(TERRARIA_ERROR_DEFAULT_BUFFER_SIZE);
    if (error == NULL)
        return MALLOC_FAILED;

    va_list args;
    va_start(args, format);

    size_t wanted = (size_t) vsnprintf(
            error, TERRARIA_ERROR_DEFAULT_BUFFER_SIZE, format, args);

    if (wanted >= TERRARIA_ERROR_DEFAULT_BUFFER_SIZE) {
        wanted++; // add the trailing NULL

        error = realloc(error, wanted);
        if (error == NULL)
            return MALLOC_FAILED;

        vsnprintf(error, wanted, format, args);
    }

    va_end(args);

    return (TerrariaError *) error;
}

TerrariaError *_terraria_make_perror(const char *message) {
    return _terraria_make_errorf("%s (%s)", message, strerror(errno));
}

TerrariaError *_terraria_make_strerror() {
    return _terraria_make_error(strerror(errno));
}

void terraria_free_error(TerrariaError *error) {
    if (error != MALLOC_FAILED)
        free(error);
}