#include "proxy.h"
#include "sockall.h"
#include <stdio.h>
#include "window.h"

typedef struct {
	const SOCKET client;
	const SOCKET server;
	const HANDLE client_event;
	const HANDLE server_event;
	const HANDLE mutex;
	window_t *const window;
} sockets_t;

typedef struct {
	const SOCKET proxy;
	const HANDLE mutex;
	window_t *const window;
} proxy_t;

#define PRINT_DUMP_CHARACTER(value) (value > 31 && value < 127 ? value : '.')

void print_dump_file(window_t *const window, window_add_t add_line, const char sender, const size_t packet_index, uint8_t *const data, const size_t count) {
	if (count < 1) {
		return;
	}
	char line[LINE_LENGTH] = {};

	size_t lines = count >> 4;
	const size_t remainder = count & 15;

	size_t index = 0;

	SYSTEMTIME time = {};
	GetLocalTime(&time);
	snprintf(line, LINE_LENGTH, "[%04d-%02d-%02dT%02d:%02d:%02d] %c #%zu", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, sender, packet_index);
	add_line(window, line);
	snprintf(line, LINE_LENGTH, "%s", "                   00 01 02 03  04 05 06 07  08 09 0a 0b  0c 0d 0e 0f");
	add_line(window, line);
	while (lines-- > 0) {
		snprintf(
			line, LINE_LENGTH,
			"%p | %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x | %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
			(void *) index,
			data[index], data[index + 1], data[index + 2], data[index + 3],
			data[index + 4], data[index + 5], data[index + 6], data[index + 7],
			data[index + 8], data[index + 9], data[index + 10], data[index + 11],
			data[index + 12], data[index + 13], data[index + 14], data[index + 15],
			PRINT_DUMP_CHARACTER(data[index]), PRINT_DUMP_CHARACTER(data[index + 1]), PRINT_DUMP_CHARACTER(data[index + 2]), PRINT_DUMP_CHARACTER(data[index + 3]),
			PRINT_DUMP_CHARACTER(data[index + 4]), PRINT_DUMP_CHARACTER(data[index + 5]), PRINT_DUMP_CHARACTER(data[index + 6]), PRINT_DUMP_CHARACTER(data[index + 7]),
			PRINT_DUMP_CHARACTER(data[index + 8]), PRINT_DUMP_CHARACTER(data[index + 9]), PRINT_DUMP_CHARACTER(data[index + 10]), PRINT_DUMP_CHARACTER(data[index + 11]),
			PRINT_DUMP_CHARACTER(data[index + 12]), PRINT_DUMP_CHARACTER(data[index + 13]), PRINT_DUMP_CHARACTER(data[index + 14]), PRINT_DUMP_CHARACTER(data[index + 15])
		);
		add_line(window, line);

		index += 16;
	}

	if (remainder > 0) {
		char *head = line;
		size_t size = LINE_LENGTH;
		const size_t length = snprintf(head, size, "%p |", (void *) index);
		if (length >= size) {
			goto add_remainder_line;
		}
		head += length;
		size -= length;
		for (size_t i = 0; i < 16; ++i) {
			if (!(i & 3)) {
				if (size > 0) {
					*(head++) = ' ';
					--size;
				} else {
					goto add_remainder_line;
				}
			}

			if (index + i < count) {
				const size_t length = snprintf(head, size, "%02x ", data[index + i]);
				if (length >= size) {
					goto add_remainder_line;
				}
				head += length;
				size -= length;
			} else {
				if (size > 0) {
					*(head++) = ' ';
					--size;
				} else {
					goto add_remainder_line;
				}
				if (size > 0) {
					*(head++) = ' ';
					--size;
				} else {
					goto add_remainder_line;
				}
				if (size > 0) {
					*(head++) = ' ';
					--size;
				} else {
					goto add_remainder_line;
				}
			}
		}
		if (size > 0) {
			*(head++) = '|';
			--size;
		} else {
			goto add_remainder_line;
		}
		if (size > 0) {
			*(head++) = ' ';
			--size;
		} else {
			goto add_remainder_line;
		}
		for (size_t i = 0; i < remainder; ++i) {
			const char value = data[index + i];
			const size_t length = snprintf(head, size, "%c", PRINT_DUMP_CHARACTER(value));
			if (length >= size) {
				goto add_remainder_line;
			}
			head += length;
			size -= length;
		}
add_remainder_line:
		add_line(window, line);
	}
	*line = 0;
	add_line(window, line);
}

DWORD client_function(void *const input) {
	const sockets_t *const sockets = (const sockets_t *) input;
	const SOCKET client = sockets->client, server = sockets->server;
	const HANDLE client_event = sockets->client_event, server_event = sockets->server_event;
	const HANDLE mutex = sockets->mutex;
	window_t *const window = sockets->window;
	const uint8_t is_adding_to_file = window->is_adding_to_file;

	char data[PROXY_DATA_LENGTH] = {};
	uint8_t retries = 0;
	size_t index = 0;
	for (;;) {
		WaitForSingleObjectEx(client_event, INFINITE, 0);

		memset(data, 0, PROXY_DATA_LENGTH);
		const int read = recv(client, data, PROXY_DATA_LENGTH, 0);
		if (read > 0) {
			const size_t length = read;
			WaitForSingleObjectEx(mutex, INFINITE, 0);
			if (is_adding_to_file) {
				print_dump_file(window, window_add_to_file, 'C', index, (uint8_t *) data, length);
			}
			if (window->is_adding_to_lines) {
				print_dump_file(window, window_add_to_lines, 'C', index, (uint8_t *) data, length);
			}
			ReleaseMutex(mutex);

			if (send_all(server, data, length) - length) {
				SetEvent(server_event);
				return 1;
			}
		} else if (++retries >= MAX_RETRIES) {
			SetEvent(server_event);
			return 1;
		} else {
			continue;
		}

		ResetEvent(client_event);
		SetEvent(server_event);
		retries = 0;
		++index;
	}

	return 0;
}

DWORD server_function(void *const input) {
	const sockets_t *const sockets = (const sockets_t *) input;
	const SOCKET client = sockets->client, server = sockets->server;
	const HANDLE client_event = sockets->client_event, server_event = sockets->server_event;
	const HANDLE mutex = sockets->mutex;
	window_t *const window = sockets->window;
	const uint8_t is_adding_to_file = window->is_adding_to_file;

	char data[PROXY_DATA_LENGTH] = {};
	uint8_t retries = 0;
	size_t index = 0;
	for (;;) {
		WaitForSingleObjectEx(server_event, INFINITE, 0);

		memset(data, 0, PROXY_DATA_LENGTH);
		const int read = recv(server, data, PROXY_DATA_LENGTH, 0);
		if (read > 0) {
			const size_t length = read;
			WaitForSingleObjectEx(mutex, INFINITE, 0);
			if (is_adding_to_file) {
				print_dump_file(window, window_add_to_file, 'S', index, (uint8_t *) data, length);
			}
			if (window->is_adding_to_lines) {
				print_dump_file(window, window_add_to_lines, 'S', index, (uint8_t *) data, length);
			}
			ReleaseMutex(mutex);

			if (send_all(client, data, length) - length) {
				SetEvent(client_event);
				return 1;
			}
		} else if (++retries >= MAX_RETRIES) {
			SetEvent(client_event);
			return 1;
		} else {
			continue;
		}

		ResetEvent(server_event);
		SetEvent(client_event);
		retries = 0;
		++index;
	}

	return 0;
}

SOCKET connect_to_server(const SOCKET client) {
	uint8_t data[SIZEOF_IPv6 + SIZEOF_PORT] = {};
	if (recv_all(client, (char *) data, 1)) {
		switch (*data) {
			case 0:
				if (recv_all_bytes(client, (char *) data, SIZEOF_IPv4 + SIZEOF_PORT) == SIZEOF_IPv4 + SIZEOF_PORT) {
					const SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
					if (server == INVALID_SOCKET) {
						SHOW_ERROR(socket);
						closesocket(client);
						return INVALID_SOCKET;
					}

					struct sockaddr_in name = {};
					name.sin_family = AF_INET;
					memcpy(&(name.sin_addr.s_addr), data, SIZEOF_IPv4);
					memcpy(&(name.sin_port), data + SIZEOF_IPv4, SIZEOF_PORT);

					if (connect(server, (SOCKADDR *) &name, sizeof(name))) {
						SHOW_ERROR(connect);
						closesocket(server);
						closesocket(client);
						return INVALID_SOCKET;
					}

					return server;
				} else {
					closesocket(client);
					return INVALID_SOCKET;
				}
				break;
			case 1:
				if (recv_all_bytes(client, (char *) data, SIZEOF_IPv6 + SIZEOF_PORT) == SIZEOF_IPv6 + SIZEOF_PORT) {
					const SOCKET server = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
					if (server == INVALID_SOCKET) {
						SHOW_ERROR(socket);
						closesocket(client);
						return INVALID_SOCKET;
					}

					struct sockaddr_in6 name = {};
					name.sin6_family = AF_INET6;
					memcpy(name.sin6_addr.u.Byte, data, SIZEOF_IPv6);
					memcpy(&(name.sin6_port), data + SIZEOF_IPv6, SIZEOF_PORT);

					if (connect(server, (SOCKADDR *) &name, sizeof(name))) {
						SHOW_ERROR(connect);
						closesocket(server);
						closesocket(client);
						return INVALID_SOCKET;
					}

					return server;
				} else {
					closesocket(client);
					return INVALID_SOCKET;
				}
				break;
			default:
				closesocket(client);
				return INVALID_SOCKET;
		}
	}

	closesocket(client);
	return INVALID_SOCKET;
}

DWORD proxy_function(void *const input) {
	const proxy_t *const data = (const proxy_t *) input;
	const SOCKET proxy = data->proxy;
	const HANDLE mutex = data->mutex;
	window_t *const window = data->window;

	char line[LINE_LENGTH] = {};
	struct sockaddr_in name = {};
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("127.0.0.1");
	name.sin_port = htons(9002);

	if (bind(proxy, (SOCKADDR *) &name, sizeof(name))) {
		SHOW_ERROR(bind);
		closesocket(proxy);
		return 1;
	}

	if (listen(proxy, 0)) {
		SHOW_ERROR(listen);
		closesocket(proxy);
		return 1;
	}
	WaitForSingleObjectEx(mutex, INFINITE, 0);
	snprintf(line, LINE_LENGTH, "Listening on %u.%u.%u.%u:%u...", name.sin_addr.S_un.S_un_b.s_b1, name.sin_addr.S_un.S_un_b.s_b2, name.sin_addr.S_un.S_un_b.s_b3, name.sin_addr.S_un.S_un_b.s_b4, ntohs(name.sin_port));
	window_add_to_lines(window, line);
	*line = 0;
	window_add_to_lines(window, line);
	ReleaseMutex(mutex);

	for (;;) {
		const SOCKET client = accept(proxy, (SOCKADDR *) NULL, (int *) NULL);
		if (client == INVALID_SOCKET) {
			SHOW_ERROR(accept);
			return 1;
		}

		const SOCKET server = connect_to_server(client);
		if (server == INVALID_SOCKET) {
			SHOW_ERROR(connect_to_server);
			closesocket(client);
			return 1;
		}

		const HANDLE client_event = CreateEventA(NULL, 1, 1, NULL);
		const HANDLE server_event = CreateEventA(NULL, 1, 0, NULL);
		const sockets_t sockets = {client, server, client_event, server_event, mutex, window};

		const HANDLE client_thread = CreateThread(NULL, 0, client_function, (void *) &sockets, 0, NULL);
		const HANDLE server_thread = CreateThread(NULL, 0, server_function, (void *) &sockets, 0, NULL);

		WaitForSingleObjectEx(server_thread, INFINITE, 0);
		WaitForSingleObjectEx(client_thread, INFINITE, 0);
		CloseHandle(server_thread);
		CloseHandle(client_thread);

		CloseHandle(server_event);
		CloseHandle(client_event);

		closesocket(server);
		closesocket(client);
	}

	return 0;
}

DWORD proxy6_function(void *const input) {
	const proxy_t *const data = (const proxy_t *) input;
	const SOCKET proxy = data->proxy;
	const HANDLE mutex = data->mutex;
	window_t *const window = data->window;

	char line[LINE_LENGTH] = {};
	struct sockaddr_in6 name = {};
	name.sin6_family = AF_INET6;
	name.sin6_addr.u.Byte[15] = 1;
	name.sin6_port = htons(9002);

	if (bind(proxy, (SOCKADDR *) &name, sizeof(name))) {
		SHOW_ERROR(bind);
		closesocket(proxy);
		return 1;
	}

	if (listen(proxy, 0)) {
		SHOW_ERROR(listen);
		closesocket(proxy);
		return 1;
	}
	WaitForSingleObjectEx(mutex, INFINITE, 0);
	snprintf(line, LINE_LENGTH, "Listening on [%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]:%u...", ntohs(name.sin6_addr.u.Word[0]), ntohs(name.sin6_addr.u.Word[1]), ntohs(name.sin6_addr.u.Word[2]), ntohs(name.sin6_addr.u.Word[3]), ntohs(name.sin6_addr.u.Word[4]), ntohs(name.sin6_addr.u.Word[5]), ntohs(name.sin6_addr.u.Word[6]), ntohs(name.sin6_addr.u.Word[7]), ntohs(name.sin6_port));
	window_add_to_lines(window, line);
	*line = 0;
	window_add_to_lines(window, line);
	ReleaseMutex(mutex);

	for (;;) {
		const SOCKET client = accept(proxy, (SOCKADDR *) NULL, (int *) NULL);
		if (client == INVALID_SOCKET) {
			SHOW_ERROR(accept);
			return 1;
		}

		const SOCKET server = connect_to_server(client);
		if (server == INVALID_SOCKET) {
			SHOW_ERROR(connect_to_server);
			closesocket(client);
			return 1;
		}

		const HANDLE client_event = CreateEventA(NULL, 1, 1, NULL);
		const HANDLE server_event = CreateEventA(NULL, 1, 0, NULL);
		const sockets_t sockets = {client, server, client_event, server_event, mutex, window};

		const HANDLE client_thread = CreateThread(NULL, 0, client_function, (void *) &sockets, 0, NULL);
		const HANDLE server_thread = CreateThread(NULL, 0, server_function, (void *) &sockets, 0, NULL);

		WaitForSingleObjectEx(server_thread, INFINITE, 0);
		WaitForSingleObjectEx(client_thread, INFINITE, 0);
		CloseHandle(server_thread);
		CloseHandle(client_thread);

		CloseHandle(server_event);
		CloseHandle(client_event);

		closesocket(server);
		closesocket(client);
	}

	return 0;
}

DWORD start_proxy(void *const input) {
	window_t *const window = (window_t *) input;
	const HANDLE mutex = CreateMutexA(NULL, 0, NULL);

	const SOCKET proxy = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (proxy == INVALID_SOCKET) {
		SHOW_ERROR(socket);
		return 1;
	}

	const SOCKET proxy6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (proxy6 == INVALID_SOCKET) {
		SHOW_ERROR(socket);
		closesocket(proxy);
		return 1;
	}

	const proxy_t proxy_data = {proxy, mutex, window};
	const proxy_t proxy6_data = {proxy6, mutex, window};

	const HANDLE proxy_thread = CreateThread(NULL, 0, proxy_function, (void *) &proxy_data, 0, NULL);
	const HANDLE proxy6_thread = CreateThread(NULL, 0, proxy6_function, (void *) &proxy6_data, 0, NULL);

	WaitForSingleObjectEx(proxy_thread, INFINITE, 0);
	WaitForSingleObjectEx(proxy6_thread, INFINITE, 0);
	CloseHandle(proxy_thread);
	CloseHandle(proxy6_thread);

	closesocket(proxy);
	closesocket(proxy6);

	CloseHandle(mutex);
	return 0;
}
