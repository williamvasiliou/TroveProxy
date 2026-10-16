#ifndef STARTER_H
#define STARTER_H
#include <stdint.h>
#include <windows.h>

#define DATA_LENGTH 128
#define DATA_READ (DATA_LENGTH - 1)

HANDLE start_inject_process(const char *const executable, const char *const dll_path, size_t *const identifier);

#endif // STARTER_H
