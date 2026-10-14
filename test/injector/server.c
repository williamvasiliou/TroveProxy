#include "input.h"
#include "sockall.h"

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

	struct sockaddr_in name = {};
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("127.0.0.1");
	name.sin_port = htons(9001);

	if (bind(server, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: bind\n", stderr);
		closesocket(server);
		WSACleanup();
		return 1;
	}

	if (listen(server, 0)) {
		fputs("Error: listen\n", stderr);
		closesocket(server);
		WSACleanup();
		return 1;
	}

	const SOCKET client = accept(server, (SOCKADDR *) NULL, (int *) NULL);
	if (client == INVALID_SOCKET) {
		fputs("Error: accept\n", stderr);
		closesocket(server);
		WSACleanup();
		return 1;
	}

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

	closesocket(client);
	closesocket(server);
	WSACleanup();
	return 0;
}
