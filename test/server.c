#include "input.h"
#include "sockall.h"

DWORD echo_function(void *const input) {
	const SOCKET client = (SOCKET) input;

	char data[DATA_LENGTH] = {};
	for (;;) {
		const size_t read = recv_all(client, data, DATA_LENGTH);
		if (read > 0) {
			printf("Client: ");
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

DWORD listen_function(void *const input) {
	const SOCKET server = (SOCKET) input;

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

		const HANDLE client_thread = CreateThread(NULL, 0, echo_function, (void *) client, 0, NULL);
		WaitForSingleObject(client_thread, INFINITE);
		CloseHandle(client_thread);
		closesocket(client);
	}

	return 0;
}

DWORD listen6_function(void *const input) {
	const SOCKET server = (SOCKET) input;

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

		const HANDLE client_thread = CreateThread(NULL, 0, echo_function, (void *) client, 0, NULL);
		WaitForSingleObject(client_thread, INFINITE);
		CloseHandle(client_thread);
		closesocket(client);
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

	const HANDLE listen_thread = CreateThread(NULL, 0, listen_function, (void *) server, 0, NULL);
	const HANDLE listen6_thread = CreateThread(NULL, 0, listen6_function, (void *) server6, 0, NULL);

	WaitForSingleObject(listen_thread, INFINITE);
	WaitForSingleObject(listen6_thread, INFINITE);
	CloseHandle(listen_thread);
	CloseHandle(listen6_thread);

	closesocket(server);
	closesocket(server6);
	WSACleanup();
	return 0;
}
