#include "input.h"
#include "sockall.h"

typedef struct {
	const size_t connection;
	const SOCKET client;
	const HANDLE connection_event;
} sockets_t;

typedef struct {
	const SOCKET server;
	size_t *const connections;
	const HANDLE connections_mutex;
	const HANDLE connection_event;
} server_t;

DWORD echo_function(void *const input) {
	const sockets_t *const sockets = (const sockets_t *) input;
	const size_t connection = sockets->connection;
	const SOCKET client = sockets->client;

	char data[DATA_LENGTH] = {};
	for (;;) {
		const size_t read = recv_all(client, data, DATA_LENGTH);
		if (read > 0) {
			printf("Client %zu: ", connection);
			for (size_t i = 0; i < read; ++i) {
				fputc(data[i], stdout);
			}

			if (send_all(client, data, read) - read) {
				fputs("\nError: send_all\n", stderr);
			} else {
				puts("\nData sent back to client.");
			}
		} else {
			break;
		}
	}

	return 0;
}

DWORD connection_function(void *const input) {
	const sockets_t *const data = (const sockets_t *) input;
	const sockets_t sockets = {data->connection, data->client, NULL};
	SetEvent(data->connection_event);

	const HANDLE client_thread = CreateThread(NULL, 0, echo_function, (void *) &sockets, 0, NULL);
	if (client_thread) {
		WaitForSingleObjectEx(client_thread, INFINITE, 0);
		CloseHandle(client_thread);

		closesocket(sockets.client);
		printf("\nClient %zu closed.\n", sockets.connection);
		return 0;
	} else {
		closesocket(sockets.client);
		printf("\nClient %zu closed.\n", sockets.connection);
		return 1;
	}
}

DWORD listen_function(void *const input) {
	const server_t *const data = (const server_t *) input;
	const SOCKET server = data->server;
	size_t *const connections = data->connections;
	const HANDLE connections_mutex = data->connections_mutex;
	const HANDLE connection_event = data->connection_event;

	struct sockaddr_in name = {};
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("127.0.0.1");
	name.sin_port = htons(9001);

	if (bind(server, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: bind\n", stderr);
		closesocket(server);
		return 1;
	}

	if (listen(server, 0)) {
		fputs("Error: listen\n", stderr);
		closesocket(server);
		return 1;
	}
	puts("Listening on 127.0.0.1:9001...\n");

	for (;;) {
		const SOCKET client = accept(server, (SOCKADDR *) NULL, (int *) NULL);
		if (client == INVALID_SOCKET) {
			fputs("Error: accept\n", stderr);
			return 1;
		}

		WaitForSingleObjectEx(connections_mutex, INFINITE, 0);
		const sockets_t sockets = {*connections, client, connection_event};
		const HANDLE connection_thread = CreateThread(NULL, 0, connection_function, (void *) &sockets, 0, NULL);
		if (connection_thread) {
			WaitForSingleObjectEx(connection_event, INFINITE, 0);
			++(*connections);
			ResetEvent(connection_event);
			CloseHandle(connection_thread);
		} else {
			closesocket(client);
		}
		ReleaseMutex(connections_mutex);
	}

	return 0;
}

DWORD listen6_function(void *const input) {
	const server_t *const data = (const server_t *) input;
	const SOCKET server = data->server;
	size_t *const connections = data->connections;
	const HANDLE connections_mutex = data->connections_mutex;
	const HANDLE connection_event = data->connection_event;

	struct sockaddr_in6 name = {};
	name.sin6_family = AF_INET6;
	name.sin6_addr.u.Byte[15] = 1;
	name.sin6_port = htons(9001);

	if (bind(server, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: bind\n", stderr);
		closesocket(server);
		return 1;
	}

	if (listen(server, 0)) {
		fputs("Error: listen\n", stderr);
		closesocket(server);
		return 1;
	}
	puts("Listening on [::1]:9001...\n");

	for (;;) {
		const SOCKET client = accept(server, (SOCKADDR *) NULL, (int *) NULL);
		if (client == INVALID_SOCKET) {
			fputs("Error: accept\n", stderr);
			return 1;
		}

		WaitForSingleObjectEx(connections_mutex, INFINITE, 0);
		const sockets_t sockets = {*connections, client, connection_event};
		const HANDLE connection_thread = CreateThread(NULL, 0, connection_function, (void *) &sockets, 0, NULL);
		if (connection_thread) {
			WaitForSingleObjectEx(connection_event, INFINITE, 0);
			++(*connections);
			ResetEvent(connection_event);
			CloseHandle(connection_thread);
		} else {
			closesocket(client);
		}
		ReleaseMutex(connections_mutex);
	}

	return 0;
}

int main() {
	WSADATA wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		fputs("Error: WSAStartup\n", stderr);
		return 1;
	}

	const SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server == INVALID_SOCKET) {
		fputs("Error: socket\n", stderr);
		WSACleanup();
		return 1;
	}

	const SOCKET server6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (server6 == INVALID_SOCKET) {
		fputs("Error: socket\n", stderr);
		closesocket(server);
		WSACleanup();
		return 1;
	}

	size_t connections = 0;
	const HANDLE connections_mutex = CreateMutexA(NULL, 0, NULL);
	const HANDLE connection_event = CreateEventA(NULL, 1, 0, NULL);
	const server_t server_data = {server, &connections, connections_mutex, connection_event};
	const server_t server6_data = {server6, &connections, connections_mutex, connection_event};

	const HANDLE listen_thread = CreateThread(NULL, 0, listen_function, (void *) &server_data, 0, NULL);
	const HANDLE listen6_thread = CreateThread(NULL, 0, listen6_function, (void *) &server6_data, 0, NULL);

	WaitForSingleObjectEx(listen_thread, INFINITE, 0);
	WaitForSingleObjectEx(listen6_thread, INFINITE, 0);
	CloseHandle(listen_thread);
	CloseHandle(listen6_thread);

	closesocket(server);
	closesocket(server6);

	CloseHandle(connections_mutex);
	CloseHandle(connection_event);
	WSACleanup();
	return 0;
}
