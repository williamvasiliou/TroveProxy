#include "input.h"
#include "sockall.h"

typedef struct {
	const SOCKET client;
	const SOCKET server;
	const HANDLE client_event;
	const HANDLE server_event;
} sockets_t;

DWORD client_function(void *const input) {
	const sockets_t *const sockets = (const sockets_t *) input;
	const SOCKET client = sockets->client, server = sockets->server;
	const HANDLE client_event = sockets->client_event, server_event = sockets->server_event;
	char data[DATA_LENGTH] = {};
	uint8_t retries = 0;

	for (;;) {
		WaitForSingleObject(client_event, INFINITE);

		const int read = recv(client, data, DATA_LENGTH, 0);
		if (read > 0) {
			const size_t length = read;
			printf("Client: ");
			for (size_t i = 0; i < length; ++i) {
				fputc(data[i], stdout);
			}
			fputc('\n', stdout);

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
	}

	return 0;
}

DWORD server_function(void *const input) {
	const sockets_t *const sockets = (const sockets_t *) input;
	const SOCKET client = sockets->client, server = sockets->server;
	const HANDLE client_event = sockets->client_event, server_event = sockets->server_event;
	char data[DATA_LENGTH] = {};
	uint8_t retries = 0;

	for (;;) {
		WaitForSingleObject(server_event, INFINITE);

		const int read = recv(server, data, DATA_LENGTH, 0);
		if (read > 0) {
			const size_t length = read;
			printf("Server: ");
			for (size_t i = 0; i < length; ++i) {
				fputc(data[i], stdout);
			}
			fputc('\n', stdout);

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
	}

	return 0;
}

int main() {
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

	struct sockaddr_in name = {};
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("127.0.0.1");
	name.sin_port = htons(9002);

	if (bind(proxy, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: bind\n", stderr);
		closesocket(proxy);
		WSACleanup();
		return 1;
	}

	if (listen(proxy, 0)) {
		fputs("Error: listen\n", stderr);
		closesocket(proxy);
		WSACleanup();
		return 1;
	}

	const SOCKET client = accept(proxy, (SOCKADDR *) NULL, (int *) NULL);
	if (client == INVALID_SOCKET) {
		fputs("Error: accept\n", stderr);
		closesocket(proxy);
		WSACleanup();
		return 1;
	}

	const SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server == INVALID_SOCKET) {
		fputs("Error: socket\n", stderr);
		closesocket(client);
		closesocket(proxy);
		WSACleanup();
		return 1;
	}

	name.sin_port = htons(9001);
	if (connect(server, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: connect\n", stderr);
		closesocket(server);
		closesocket(client);
		closesocket(proxy);
		WSACleanup();
		return 1;
	}

	const HANDLE client_event = CreateEventA(NULL, 1, 1, NULL);
	const HANDLE server_event = CreateEventA(NULL, 1, 0, NULL);
	const sockets_t sockets = {client, server, client_event, server_event};

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
	closesocket(proxy);
	WSACleanup();
	return 0;
}
