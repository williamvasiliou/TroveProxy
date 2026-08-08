#ifndef STARTER_H
#define STARTER_H
#include <stdint.h>

#define DATA_LENGTH 128
#define DATA_READ (DATA_LENGTH - 1)

uint8_t start_inject_process(const char *const executable, const char *const dll_path);

#endif // STARTER_H
