#include "input.h"
#include "sockall.h"

SOCKET menu_connect() {
	const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == INVALID_SOCKET) {
		fputs("Error: socket\n", stderr);
		return INVALID_SOCKET;
	}

	struct sockaddr_in name = {};
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("127.0.0.1");
	name.sin_port = htons(9001);

	if (connect(client, (SOCKADDR *) &name, sizeof(name))) {
		fputs("Error: connect\n", stderr);
		closesocket(client);
		return INVALID_SOCKET;
	}

	return client;
}

#define MENU_DISCONNECT(client)		\
	if (client != INVALID_SOCKET) {	\
		closesocket(client);	\
		client = INVALID_SOCKET;\
	}

void menu() {
	char data[DATA_LENGTH] = {};
	SOCKET client = INVALID_SOCKET;

	size_t read = 0;
	for (;;) {
		if (feof(stdin) || ferror(stdin)) {
			MENU_DISCONNECT(client);
			return;
		}
		printf("\n1. Connect\n2. Send data\n3. Disconnect\nQ. Quit\n\nPlease select: ");
		read = get_data(data, 2);

		if (read == 2) {
			if (data[1] == '\n') {
				switch (*data) {
					case '1':
						if (client == INVALID_SOCKET) {
							if ((client = menu_connect()) != INVALID_SOCKET) {
								puts("Connected to 127.0.0.1:9001");
							} else {
								fputs("Error: menu_connect\n", stderr);
							}
						} else {
							fputs("Error: connected already!\n", stderr);
						}
						break;
					case '2':
						if (client != INVALID_SOCKET) {
							printf("Enter data: ");
							read = get_data(data, DATA_READ);
							if (read < DATA_READ) {
								if (read > 0 && data[read - 1] == '\n') {
									data[read] = 0;
									const size_t sent = send_all(client, data, read);
									if (read - sent) {
										fputs("Error: send_all\n", stderr);
									} else {
										if (recv_all(client, data, read) - read) {
											fputs("Error: recv_all\n", stderr);
										} else {
											printf("Server: %s", data);
										}
									}
								}
							} else {
								fputs("Error: input too long!\n", stderr);
							}
						} else {
							fputs("Error: not connected!\n", stderr);
						}
						break;
					case '3':
						MENU_DISCONNECT(client);
						break;
					case 'Q':
					case 'q':
						MENU_DISCONNECT(client);
						return;
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
}

int main() {
	WSADATA wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		fputs("Error: WSAStartup\n", stderr);
		return 1;
	}

	menu();
	WSACleanup();
	return 0;
}
