#ifndef INPUT_H
#define INPUT_H
#include <stdio.h>

#define DATA_READ 127
#define DATA_LENGTH 128

size_t get_data(char *const data, const size_t length) {
	if (length < 1) {
		return 0;
	}

	size_t read = 0;
	char line = 0;
	int character = fgetc(stdin);
	if (character == '\n') {
		line = 1;
	}

	while (character != EOF && read < length) {
		data[read++] = character;
		if (line) {
			break;
		}

		character = fgetc(stdin);
		if (character == '\n') {
			line = 1;
		}
	}

	if (read == length && character != '\n' && data[read - 1] != '\n') {
		while (fgetc(stdin) != '\n');
	}

	return read;
}

#endif // INPUT_H
