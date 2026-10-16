#include "offsets.h"
#include "sockall.h"
#include <windows.h>

__declspec(dllexport) void initialize(void *const page) {
	const uintmax_t code_page = *(uintmax_t *) page;

	MEMORY_BASIC_INFORMATION memory = {};
	VirtualQuery((LPCVOID) code_page, &memory, sizeof(MEMORY_BASIC_INFORMATION));
	void *const query_address = memory.BaseAddress;
	const size_t query_size = memory.RegionSize;

	DWORD protect = 0;
	VirtualProtect(query_address, query_size, PAGE_READWRITE, &protect);

	const HANDLE mutex = CreateMutexA(NULL, 0, NULL);
	*((uint64_t *) (code_page + CONNECT_CODE_OFFSET_3)) = (uint64_t) mutex;
	*((uint64_t *) (code_page + CONNECT_CODE_OFFSET_10)) = (uint64_t) send_all;
	*((uint64_t *) (code_page + CONNECT_CODE_OFFSET_11)) = (uint64_t) send_all;
	*((uint64_t *) (code_page + CONNECT_CODE_OFFSET_12)) = (uint64_t) send_all;
	*((uint64_t *) (code_page + CONNECT_CODE_OFFSET_13)) = (uint64_t) mutex;

	VirtualProtect(query_address, query_size, protect, &protect);
}
