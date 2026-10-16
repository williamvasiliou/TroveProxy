#ifndef SOCKALL_H
#define SOCKALL_H
#include <stdint.h>
#include <winsock2.h>
#include <ws2ipdef.h>

#define MAX_RETRIES 10

#define SIZEOF_IPv4 4
#define SIZEOF_IPv6 16
#define SIZEOF_PORT 2

size_t send_all(const SOCKET client, const char *const buffer, const size_t length) {
	if (length < 1) {
		return 0;
	}

	uint8_t retries = 0;
	size_t total_sent = 0;
	const int sent = send(client, buffer, length, 0);

	if (sent > 0) {
		total_sent += sent;
	} else if (++retries >= MAX_RETRIES) {
		return total_sent;
	}

	while (total_sent < length) {
		const int sent = send(client, buffer + total_sent, length - total_sent, 0);
		if (sent > 0) {
			total_sent += sent;
		} else if (++retries >= MAX_RETRIES) {
			break;
		}
	}

	return total_sent;
}

size_t recv_all(const SOCKET client, char *const buffer, const size_t length) {
	if (length < 1) {
		return 0;
	}

	uint8_t retries = 0;
	size_t total_read = 0;
	const int read = recv(client, buffer, length, 0);

	if (read > 0) {
		total_read += read;
	} else if (++retries >= MAX_RETRIES) {
		return total_read;
	}

	if (total_read > 0 && buffer[total_read - 1] == '\n') {
		return total_read;
	}

	while (total_read < length) {
		const int read = recv(client, buffer + total_read, length - total_read, 0);
		if (read > 0) {
			total_read += read;
		} else if (++retries >= MAX_RETRIES) {
			break;
		}

		if (total_read > 0 && buffer[total_read - 1] == '\n') {
			return total_read;
		}
	}

	return total_read;
}

size_t recv_all_bytes(const SOCKET client, char *const buffer, const size_t length) {
	if (length < 1) {
		return 0;
	}

	uint8_t retries = 0;
	size_t total_read = 0;
	const int read = recv(client, buffer, length, 0);

	if (read > 0) {
		total_read += read;
	} else if (++retries >= MAX_RETRIES) {
		return total_read;
	}

	while (total_read < length) {
		const int read = recv(client, buffer + total_read, length - total_read, 0);
		if (read > 0) {
			total_read += read;
		} else if (++retries >= MAX_RETRIES) {
			break;
		}
	}

	return total_read;
}

#endif // SOCKALL_H
