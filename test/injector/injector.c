#include "input.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <tlhelp32.h>

#define DEFAULT_ALLOC_SIZE 16

typedef struct {
	DWORD id;
	const char *name;
	HGLOBAL global;
} menu_entry_t;

typedef struct {
	menu_entry_t **entries;
	HGLOBAL global;
	size_t index;
	size_t size;
} menu_t;

char *copy_name(const char *name, HGLOBAL *out_global) {
	if (!name || !out_global) {
		return NULL;
	}

	HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, DEFAULT_ALLOC_SIZE);
	char *copy = (char *) GlobalLock(global);
	size_t copied = 0;
	size_t size = DEFAULT_ALLOC_SIZE;
	if (copy) {
		char value = *name;
		while (value) {
			if (copied < size) {
				copy[copied++] = value;
			} else {
				HGLOBAL global_array = GlobalReAlloc(copy, 2 * size, GMEM_MOVEABLE);
				if (!global_array) {
					GlobalUnlock(global);
					GlobalFree(global);
					return NULL;
				}
				global = global_array;
				copy = (char *) GlobalLock(global_array);
				copy[copied++] = value;
				size *= 2;
			}
			value = *(++name);
		}

		if (copied < size) {
			copy[copied] = value;
		} else {
			HGLOBAL global_array = GlobalReAlloc(copy, 2 * size, GMEM_MOVEABLE);
			if (!global_array) {
				GlobalUnlock(global);
				GlobalFree(global);
				return NULL;
			}
			global = global_array;
			copy = (char *) GlobalLock(global_array);
			copy[copied] = value;
		}

		*out_global = global;
		return copy;
	}

	return NULL;
}

void print_menu(const menu_t *menu) {
	if (!menu) {
		return;
	}

	const menu_entry_t **const entries = (const menu_entry_t **) menu->entries;
	if (!entries) {
		return;
	}

	fputc('\n', stdout);
	const size_t size = menu->index;
	for (size_t i = 0; i < size;) {
		const menu_entry_t *const entry = entries[i++];
		if (entry) {
			printf("%zu. %ld %s\n", i, entry->id, entry->name);
		} else {
			puts("Invalid row.");
		}
	}
	printf("Q. Quit\n\nPlease select: ");
}

void free_menu(const menu_t *menu) {
	if (!menu) {
		return;
	}

	const menu_entry_t **const entries = (const menu_entry_t **) menu->entries;
	if (!entries) {
		return;
	}
	const HGLOBAL global = menu->global;

	const size_t size = menu->index;
	for (size_t i = 0; i < size; ++i) {
		const menu_entry_t *const entry = entries[i];
		if (entry) {
			const char *const name = entry->name;
			if (name) {
				const HGLOBAL global = entry->global;
				GlobalUnlock(global);
				GlobalFree(global);
			}
			GlobalFree((void *) entry);
		} else {
			puts("Invalid row.");
		}
	}
	GlobalUnlock(global);
	GlobalFree(global);
	GlobalFree((void *) menu);
}

menu_t *add_to_menu(menu_t *menu, const DWORD id, const char *const name) {
	if (!name) {
		return menu;
	}

	if (!menu) {
		return NULL;
	}

	const size_t index = menu->index;
	const size_t size = menu->size;
	if (index >= size) {
		HGLOBAL global = GlobalReAlloc(menu->entries, 2 * size * sizeof(menu_entry_t *), GMEM_MOVEABLE);
		if (!global) {
			free_menu(menu);
			return NULL;
		}
		menu->global = global;
		menu->entries = (menu_entry_t **) GlobalLock(global);
		menu->size *= 2;
	}

	if (index >= menu->size) {
		free_menu(menu);
		return NULL;
	}

	menu_entry_t *const entry = (menu_entry_t *) GlobalAlloc(GMEM_FIXED, sizeof(menu_entry_t));
	if (!entry) {
		free_menu(menu);
		return NULL;
	}

	HGLOBAL global = NULL;
	const char *const copy = copy_name(name, &global);
	if (!copy) {
		GlobalFree(entry);
		free_menu(menu);
		return NULL;
	}

	entry->id = id;
	entry->name = copy;
	entry->global = global;
	menu->entries[index] = entry;

	++menu->index;
	return menu;
}

menu_t *create_menu(const char *const menu_name) {
	menu_t *menu = (menu_t *) GlobalAlloc(GMEM_FIXED, sizeof(menu_t));
	if (!menu) {
		return NULL;
	}

	HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, DEFAULT_ALLOC_SIZE * sizeof(menu_entry_t *));
	if (!global) {
		GlobalFree(menu);
		return NULL;
	}

	menu->entries = (menu_entry_t **) GlobalLock(global);
	menu->global = global;
	menu->index = 0;
	menu->size = DEFAULT_ALLOC_SIZE;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		fputs("Error: CreateToolhelp32Snapshot\n", stderr);
		return NULL;
	}

	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(snapshot, &entry)) {
		const char *const name = entry.szExeFile;
		if (!stricmp(name, menu_name)) {
			menu = add_to_menu(menu, entry.th32ProcessID, name);
			if (!menu) {
				goto exit_create_menu;
			}
		}

		while (Process32Next(snapshot, &entry)) {
			const char *const name = entry.szExeFile;
			if (!stricmp(name, menu_name)) {
				menu = add_to_menu(menu, entry.th32ProcessID, name);
				if (!menu) {
					goto exit_create_menu;
				}
			}
		}
	}

exit_create_menu:
	CloseHandle(snapshot);
	return menu;
}

uint8_t remote_read_byte(const HANDLE process, const uintmax_t address) {
	uint8_t byte = 0;
	ReadProcessMemory(process, (void *) address, &byte, 1, NULL);
	return byte;
}

#define REMOTE_READ_WORD(process, address)		\
	(remote_read_byte(process, address)	       |\
	(remote_read_byte(process, address + 1) << 8))

#define REMOTE_READ_DWORD(process, address)		\
	(remote_read_byte(process, address)	       |\
	(remote_read_byte(process, address + 1) << 8)  |\
	(remote_read_byte(process, address + 2) << 16) |\
	(remote_read_byte(process, address + 3) << 24))

size_t remote_read_string(const HANDLE process, uintmax_t address, char *data, const size_t size) {
	if (size < 1) {
		return 0;
	}

	size_t read = data[0] = 0;
	while (read < size) {
		ReadProcessMemory(process, (void *) address, data, 1, NULL);
		++address;
		++read;
		if (*data) {
			++data;
		} else {
			break;
		}
	}

	return read;
}

size_t search_name_pointer_table(const HANDLE process, const uintmax_t base_address, uintmax_t table_address, const char *const value) {
	char data[DATA_LENGTH] = {};

	size_t index = 0;
	uint32_t offset = REMOTE_READ_DWORD(process, table_address);
	while (offset != 0) {
		data[remote_read_string(process, base_address + offset, data, DATA_READ)] = 0;
		if (!strcmp(data, value)) {
			return index;
		}

		table_address += 4;
		offset = REMOTE_READ_DWORD(process, table_address);
		++index;
	}

	return -1;
}

void *inject_alloc_connect(const HANDLE process, const uintmax_t connect, const char *const path, const size_t size, const uintmax_t load_library_address) {
	void *const page = VirtualAllocEx(process, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	size_t sent = 0;
	WriteProcessMemory(process, page, &connect, sizeof(uintmax_t), &sent);
	if (sent == sizeof(uintmax_t)) {
		WriteProcessMemory(process, (void *) ((uintmax_t) page + sizeof(uintmax_t)), path, size, &sent);
		if (sent == size) {
			const HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE) load_library_address, (void *) ((uintmax_t) page + sizeof(uintmax_t)), 0, NULL);
			if (thread) {
				CloseHandle(thread);
			} else {
				fputs("Error: inject_alloc_connect\n", stderr);
				VirtualFreeEx(process, page, 0, MEM_DECOMMIT | MEM_RELEASE);
				return NULL;
			}
		} else {
			VirtualFreeEx(process, page, 0, MEM_DECOMMIT | MEM_RELEASE);
			return NULL;
		}
	} else {
		VirtualFreeEx(process, page, 0, MEM_DECOMMIT | MEM_RELEASE);
		return NULL;
	}
	return page;
}

uintmax_t find_export_function(const HANDLE process, const uintmax_t base_address, const char *const name) {
	const uint32_t signature_offset = REMOTE_READ_DWORD(process, base_address + 0x3c);
	const uintmax_t optional_header_address = base_address + signature_offset + 0x18;
	const uint16_t optional_magic = REMOTE_READ_WORD(process, optional_header_address);
	const uint8_t data_directories_offset = optional_magic == 0x10b ? 0x5c : 0x6c;
	const uint32_t data_directories = REMOTE_READ_DWORD(process, optional_header_address + data_directories_offset);
	if (data_directories > 0) {
		const uint32_t export_directory_table_offset = REMOTE_READ_DWORD(process, optional_header_address + data_directories_offset + 4);
		if (export_directory_table_offset > 0) {
			const uintmax_t export_directory_table_address = base_address + export_directory_table_offset;
			const uintmax_t export_address_table_address = base_address + REMOTE_READ_DWORD(process, export_directory_table_address + 0x1c);
			const uintmax_t export_name_pointer_table_address = base_address + REMOTE_READ_DWORD(process, export_directory_table_address + 0x20);
			const uintmax_t export_ordinal_table_address = base_address + REMOTE_READ_DWORD(process, export_directory_table_address + 0x24);
			const size_t ordinal_index = search_name_pointer_table(process, base_address, export_name_pointer_table_address, name);
			if (ordinal_index != (size_t) -1) {
				const uint16_t address_index = REMOTE_READ_WORD(process, export_ordinal_table_address + ordinal_index * sizeof(uint16_t));
				return base_address + REMOTE_READ_DWORD(process, export_address_table_address + address_index * sizeof(uint32_t));
			}
		}
	}
	return 0;
}

void find_export_base_addresses(const DWORD id, uintmax_t *const out_kernelbase, uintmax_t *const out_ws2_32) {
	if (!out_kernelbase && !out_ws2_32) {
		return;
	}

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, id);
	if (snapshot == INVALID_HANDLE_VALUE) {
		fputs("Error: CreateToolhelp32Snapshot\n", stderr);
		return;
	}
	uintmax_t kernelbase = 0;
	uintmax_t ws2_32 = 0;

	MODULEENTRY32 entry = {};
	entry.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(snapshot, &entry)) {
		const char *const name = entry.szModule;
		if (!kernelbase && !stricmp(name, "kernelbase.dll")) {
			kernelbase = (uintmax_t) entry.modBaseAddr;
		} else if (!ws2_32 && !stricmp(name, "ws2_32.dll")) {
			ws2_32 = (uintmax_t) entry.modBaseAddr;
		}
		while (Module32Next(snapshot, &entry)) {
			const char *const name = entry.szModule;
			if (!kernelbase && !stricmp(name, "kernelbase.dll")) {
				kernelbase = (uintmax_t) entry.modBaseAddr;
			} else if (!ws2_32 && !stricmp(name, "ws2_32.dll")) {
				ws2_32 = (uintmax_t) entry.modBaseAddr;
			}

			if (kernelbase && ws2_32) {
				break;
			}
		}
	}

	CloseHandle(snapshot);
	*out_kernelbase = kernelbase;
	*out_ws2_32 = ws2_32;
}

uintmax_t find_export_base_address(const DWORD id, const char *const module) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, id);
	if (snapshot == INVALID_HANDLE_VALUE) {
		fputs("Error: CreateToolhelp32Snapshot\n", stderr);
		return 0;
	}
	uintmax_t address = 0;

	MODULEENTRY32 entry = {};
	entry.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(snapshot, &entry)) {
		const char *const name = entry.szModule;
		if (!stricmp(name, module)) {
			address = (uintmax_t) entry.modBaseAddr;
			goto exit_find_export_base_address;
		}
		while (Module32Next(snapshot, &entry)) {
			const char *const name = entry.szModule;
			if (!stricmp(name, module)) {
				address = (uintmax_t) entry.modBaseAddr;
				goto exit_find_export_base_address;
			}
		}
	}

exit_find_export_base_address:
	CloseHandle(snapshot);
	return address;
}

static const char *const path = "C:\\path\\to\\dll";

void show_menu(const menu_t *const menu) {
	char data[DATA_LENGTH] = {};

	size_t read = 0;
	for (;;) {
		if (feof(stdin) || ferror(stdin)) {
			return;
		}
		print_menu(menu);
		read = get_data(data, DATA_READ);

		if (read > 1) {
			if (data[1] == '\n') {
				const char input = *data;
				if (input < 48 || input > 57) {
					switch (input) {
						case 'Q':
						case 'q':
							return;
						default:
							fputs("Error: Invalid input.\n", stderr);
							continue;
					}
				}
			}

			data[read] = 0;
			size_t index = strtoull(data, NULL, 10);
			if (index > 0 && index <= menu->index) {
				const DWORD id = menu->entries[index - 1]->id;
				uintmax_t kernelbase = 0;
				uintmax_t ws2_32 = 0;
				find_export_base_addresses(id, &kernelbase, &ws2_32);
				if (kernelbase && ws2_32) {
					const HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, 0, id);
					if (process) {
						const uintmax_t LoadLibraryA = find_export_function(process, kernelbase, "LoadLibraryA");
						const uintmax_t connect = find_export_function(process, ws2_32, "connect");
						void *const page = inject_alloc_connect(process, connect, path, strlen(path) + 1, LoadLibraryA);
						if (page) {
							uintmax_t patch_dll = find_export_base_address(id, "patch_dll.dll");
							while (!patch_dll) {
								patch_dll = find_export_base_address(id, "patch_dll.dll");
							}

							const uintmax_t patch_connect = find_export_function(process, patch_dll, "patch_connect");
							const HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE) patch_connect, page, 0, NULL);
							if (thread) {
								CloseHandle(thread);
							} else {
								fputs("Error: CreateRemoteThread\n", stderr);
								VirtualFreeEx(process, page, 0, MEM_DECOMMIT | MEM_RELEASE);
							}
						}
						CloseHandle(process);
					} else {
						fputs("Error: OpenProcess\n", stderr);
					}
				}
			} else {
				fputs("Error: Invalid index.\n", stderr);
			}
		} else {
			fputs("Error: Invalid input.\n", stderr);
		}
	}
}

int main() {
	char data[DATA_LENGTH] = {};
	const menu_t *menu = NULL;

	size_t read = 0;
	for (;;) {
		if (feof(stdin) || ferror(stdin)) {
			return 0;
		}
		printf("\n1. Inject processes by name\nQ. Quit\n\nPlease select: ");
		read = get_data(data, 2);

		if (read == 2) {
			if (data[1] == '\n') {
				switch (*data) {
					case '1':
						printf("Enter name: ");
						read = get_data(data, DATA_READ);
						if (read > 0) {
							data[read - 1] = 0;
							data[read] = 0;
							menu = (const menu_t *) create_menu(data);
							if (menu) {
								show_menu(menu);
								free_menu(menu);
							} else {
								fputs("Error: create_menu\n", stderr);
							}
						}
						break;
					case 'Q':
					case 'q':
						return 0;
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
