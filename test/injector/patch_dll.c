#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define DATA_LENGTH 128
static char data[DATA_LENGTH] = {};

#define CONNECT_CODE_OFFSET 30
#define CONNECT_CODE_LENGTH 41
static uint8_t connect_code[CONNECT_CODE_LENGTH] = {
	// mov rax, rsp
	0x48, 0x8B, 0xC4,
	// mov qword ptr ds:[rax+8], rbx
	0x48, 0x89, 0x58, 0x08,
	// mov qword ptr ds:[rax+10], rbp
	0x48, 0x89, 0x68, 0x10,
	// mov qword ptr ds:[rax+18], rsi
	0x48, 0x89, 0x70, 0x18,
	// mov word ptr ds:[rdx+2], 2A23
	0x66, 0xC7, 0x42, 0x02, 0x23, 0x2A,
	// mov dword ptr ds:[rdx+4], 100007F
	0xC7, 0x42, 0x04, 0x7F, 0x00, 0x00, 0x01,
	// mov rax, <ws2_32.connect+C>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// jmp rax
	0xFF, 0xE0,
	// nop
	0x90,
};

#define HOOK_CODE_OFFSET 2
#define HOOK_CODE_LENGTH 15
static uint8_t hook_code[HOOK_CODE_LENGTH] = {
	// mov rax, <hook>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// jmp rax
	0xFF, 0xE0,
	// mov rax, rsp
	0x48, 0x8B, 0xC4,
};

#define CONNECT_PAGE_SIZE 0x1000

__declspec(dllexport) void patch_connect(uintmax_t *const page) {
	void *const connect_page = VirtualAlloc(NULL, CONNECT_PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (connect_page) {
		*((uint64_t *) (hook_code + HOOK_CODE_OFFSET)) = (uint64_t) connect_page;
		const uintmax_t connect_address = *page;
		*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET)) = connect_address + 0xc;

		MEMORY_BASIC_INFORMATION memory = {};
		VirtualQuery((LPCVOID) connect_address, &memory, sizeof(MEMORY_BASIC_INFORMATION));
		void *const query_address = memory.BaseAddress;
		const size_t query_size = memory.RegionSize;

		DWORD protect = 0;
		if (VirtualProtect(query_address, query_size, PAGE_EXECUTE_READWRITE, &protect)) {
			MessageBoxA(NULL, "VirtualProtect 1", NULL, 0);
		}

		memcpy((void *) connect_address, hook_code, HOOK_CODE_LENGTH);
		memcpy(connect_page, connect_code, CONNECT_CODE_LENGTH);
		if (VirtualProtect(query_address, query_size, protect, &protect)) {
			MessageBoxA(NULL, "VirtualProtect 2", NULL, 0);
		}

		if (VirtualProtect(connect_page, CONNECT_PAGE_SIZE, PAGE_EXECUTE_READ, &protect)) {
			MessageBoxA(NULL, "VirtualProtect 3", NULL, 0);
		}
		snprintf(data, DATA_LENGTH, "query_address: %p\nquery_size: %p\n", query_address, (void *) query_size);
		MessageBoxA(NULL, data, NULL, 0);
		snprintf(data, DATA_LENGTH, "connect_address: %p\nconnect_page: %p\n", (void *) connect_address, (void *) connect_page);
		MessageBoxA(NULL, data, NULL, 0);
	}
}
