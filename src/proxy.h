#ifndef PROXY_H
#define PROXY_H
#include <winsock2.h>
#include <windows.h>

#define PROXY_DATA_LENGTH 65536

DWORD start_proxy(void *const input);

#endif // PROXY_H
