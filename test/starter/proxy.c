#include "input.h"
#include "sockall.h"

typedef struct {
	const SOCKET client;
	const SOCKET server;
	const HANDLE client_event;
	const HANDLE server_event;
	FILE *const stream;
} sockets_t;

typedef struct {
	const SOCKET proxy;
	FILE *const stream;
} proxy_t;

#define PRINT_DUMP_CHARACTER(value) (value > 31 && value < 127 ? value : '.')

void print_dump_file(FILE *const stream, const char sender, const size_t packet_index, uint8_t *const data, const size_t size) {
	if (size < 1) {
		return;
	}

	size_t lines = size >> 4;
	const size_t remainder = size & 15;

	size_t index = 0;

	SYSTEMTIME time = {};
	GetLocalTime(&time);
	fprintf(stream, "[%04d-%02d-%02dT%02d:%02d:%02d] %c #%zu\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, sender, packet_index);
	fputs("                   00 01 02 03  04 05 06 07  08 09 0a 0b  0c 0d 0e 0f\n", stream);
	while (lines-- > 0) {
		fprintf(
			stream,
			"%p | %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x | %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
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

		index += 16;
	}

	if (remainder > 0) {
		fprintf(stream, "%p |", (void *) index);
		for (size_t i = 0; i < 16; ++i) {
			if (!(i & 3)) {
				fputc(' ', stream);
			}

			if (index + i < size) {
				fprintf(stream, "%02x ", data[index + i]);
			} else {
				fputc(' ', stream);
				fputc(' ', stream);
				fputc(' ', stream);
			}
		}
		fputc('|', stream);
		fputc(' ', stream);
		for (size_t i = 0; i < remainder; ++i) {
			const char value = data[index + i];
			fprintf(stream, "%c", PRINT_DUMP_CHARACTER(value));
		}
		fputc('\n', stream);
	}
	fputc('\n', stream);
}

DWORD client_function(void *const input) {
	const sockets_t *const sockets = (const sockets_t *) input;
	const SOCKET client = sockets->client, server = sockets->server;
	const HANDLE client_event = sockets->client_event, server_event = sockets->server_event;
	FILE *const stream = sockets->stream;

	char data[DATA_LENGTH] = {};
	uint8_t retries = 0;
	size_t index = 0;
	for (;;) {
		WaitForSingleObject(client_event, INFINITE);

		const int read = recv(client, data, DATA_LENGTH, 0);
		if (read > 0) {
			const size_t length = read;
			print_dump_file(stream, 'C', index, (uint8_t *) data, length);
			if (stream != stdout) {
				fflush(stream);
				print_dump_file(stdout, 'C', index, (uint8_t *) data, length);
			}

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
	FILE *const stream = sockets->stream;

	char data[DATA_LENGTH] = {};
	uint8_t retries = 0;
	size_t index = 0;
	for (;;) {
		WaitForSingleObject(server_event, INFINITE);

		const int read = recv(server, data, DATA_LENGTH, 0);
		if (read > 0) {
			const size_t length = read;
			print_dump_file(stream, 'S', index, (uint8_t *) data, length);
			if (stream != stdout) {
				fflush(stream);
				print_dump_file(stdout, 'S', index, (uint8_t *) data, length);
			}

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
						fputs("Error: socket\n", stderr);
						closesocket(client);
						return INVALID_SOCKET;
					}

					struct sockaddr_in name = {};
					name.sin_family = AF_INET;
					memcpy(&(name.sin_addr.s_addr), data, SIZEOF_IPv4);
					memcpy(&(name.sin_port), data + SIZEOF_IPv4, SIZEOF_PORT);

					if (connect(server, (SOCKADDR *) &name, sizeof(name))) {
						fputs("Error: connect\n", stderr);
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
						fputs("Error: socket\n", stderr);
						closesocket(client);
						return INVALID_SOCKET;
					}

					struct sockaddr_in6 name = {};
					name.sin6_family = AF_INET6;
					memcpy(name.sin6_addr.u.Byte, data, SIZEOF_IPv6);
					memcpy(&(name.sin6_port), data + SIZEOF_IPv6, SIZEOF_PORT);

					if (connect(server, (SOCKADDR *) &name, sizeof(name))) {
						fputs("Error: connect\n", stderr);
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
	FILE *const stream = data->stream;

	struct sockaddr_in name = {};
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("127.0.0.1");
	name.sin_port = htons(9002);

	if (bind(proxy, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: bind\n", stderr);
		closesocket(proxy);
		return 1;
	}

	if (listen(proxy, 0)) {
		fputs("Error: listen\n", stderr);
		closesocket(proxy);
		return 1;
	}
	puts("Listening on 127.0.0.1:9002...\n");

	for (;;) {
		const SOCKET client = accept(proxy, (SOCKADDR *) NULL, (int *) NULL);
		if (client == INVALID_SOCKET) {
			fputs("Error: accept\n", stderr);
			return 1;
		}

		const SOCKET server = connect_to_server(client);
		if (server == INVALID_SOCKET) {
			fputs("Error: connect_to_server\n", stderr);
			closesocket(client);
			return 1;
		}

		const HANDLE client_event = CreateEventA(NULL, 1, 1, NULL);
		const HANDLE server_event = CreateEventA(NULL, 1, 0, NULL);
		const sockets_t sockets = {client, server, client_event, server_event, stream};

		const HANDLE client_thread = CreateThread(NULL, 0, client_function, (void *) &sockets, 0, NULL);
		const HANDLE server_thread = CreateThread(NULL, 0, server_function, (void *) &sockets, 0, NULL);

		WaitForSingleObject(server_thread, INFINITE);
		WaitForSingleObject(client_thread, INFINITE);
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
	FILE *const stream = data->stream;

	struct sockaddr_in6 name = {};
	name.sin6_family = AF_INET6;
	name.sin6_addr.u.Byte[15] = 1;
	name.sin6_port = htons(9002);

	if (bind(proxy, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: bind\n", stderr);
		closesocket(proxy);
		return 1;
	}

	if (listen(proxy, 0)) {
		fputs("Error: listen\n", stderr);
		closesocket(proxy);
		return 1;
	}
	puts("Listening on [::1]:9002...\n");

	for (;;) {
		const SOCKET client = accept(proxy, (SOCKADDR *) NULL, (int *) NULL);
		if (client == INVALID_SOCKET) {
			fputs("Error: accept\n", stderr);
			return 1;
		}

		const SOCKET server = connect_to_server(client);
		if (server == INVALID_SOCKET) {
			fputs("Error: connect_to_server\n", stderr);
			closesocket(client);
			return 1;
		}

		const HANDLE client_event = CreateEventA(NULL, 1, 1, NULL);
		const HANDLE server_event = CreateEventA(NULL, 1, 0, NULL);
		const sockets_t sockets = {client, server, client_event, server_event, stream};

		const HANDLE client_thread = CreateThread(NULL, 0, client_function, (void *) &sockets, 0, NULL);
		const HANDLE server_thread = CreateThread(NULL, 0, server_function, (void *) &sockets, 0, NULL);

		WaitForSingleObject(server_thread, INFINITE);
		WaitForSingleObject(client_thread, INFINITE);
		CloseHandle(server_thread);
		CloseHandle(client_thread);

		CloseHandle(server_event);
		CloseHandle(client_event);

		closesocket(server);
		closesocket(client);
	}

	return 0;
}

FILE *menu() {
	char data[DATA_LENGTH] = {};
	SYSTEMTIME time = {};
	FILE *stream = NULL;

	size_t read = 0;
	for (;;) {
		if (feof(stdin) || ferror(stdin)) {
			return NULL;
		}
		printf("\n1. Capture to file\n2. Capture to standard output\nQ. Quit\n\nPlease select: ");
		read = get_data(data, 2);

		if (read == 2) {
			if (data[1] == '\n') {
				switch (*data) {
					case '1':
						GetLocalTime(&time);
						snprintf(data, DATA_LENGTH, "%04d-%02d-%02d_%02d_%02d_%02d_packet_log.txt", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
						stream = fopen(data, "r");
						if (stream) {
							fputs("Error: file exists!\n", stderr);
							fclose(stream);
						} else {
							printf("File: %s\n\n", data);
							return fopen(data, "w");
						}
						break;
					case '2':
						return stdout;
					case 'Q':
					case 'q':
						return NULL;
					default:
						fputs("Error: Invalid input.\n", stderr);
						break;
				}
			} else {
				fputs("Error: Invalid input.\n", stderr);
			}
		} else {
			fputs("Error: Invalid input.\n", stderr);
		}
	}

	return NULL;
}

int main() {
	FILE *const stream = menu();
	if (!stream) {
		return 1;
	}

	WSADATA wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		fputs("Error: WSAStartup\n", stderr);
		return 1;
	}

	const SOCKET proxy = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (proxy == INVALID_SOCKET) {
		fputs("Error: socket\n", stderr);
		WSACleanup();
		return 1;
	}

	const SOCKET proxy6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (proxy6 == INVALID_SOCKET) {
		fputs("Error: socket\n", stderr);
		closesocket(proxy);
		WSACleanup();
		return 1;
	}

	const proxy_t proxy_data = {proxy, stream};
	const proxy_t proxy6_data = {proxy6, stream};

	const HANDLE proxy_thread = CreateThread(NULL, 0, proxy_function, (void *) &proxy_data, 0, NULL);
	const HANDLE proxy6_thread = CreateThread(NULL, 0, proxy6_function, (void *) &proxy6_data, 0, NULL);

	WaitForSingleObject(proxy_thread, INFINITE);
	WaitForSingleObject(proxy6_thread, INFINITE);
	CloseHandle(proxy_thread);
	CloseHandle(proxy6_thread);

	closesocket(proxy);
	closesocket(proxy6);
	WSACleanup();

	fclose(stream);
	return 0;
}
