#include "offsets.h"
#include "starter.h"
#include "window.h"

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

#define DEFAULT_PAGE_SIZE 0x1000

void *inject_alloc(const HANDLE process, const char *const path, const size_t size, const uintmax_t code_page, const uintmax_t load_library_address) {
	void *const page = VirtualAllocEx(process, NULL, DEFAULT_PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (page) {
		size_t sent = 0;
		WriteProcessMemory(process, page, &code_page, sizeof(uintmax_t), &sent);
		if (sent == sizeof(uintmax_t)) {
			void *const page_path = (void *) ((uintmax_t) page + sizeof(uintmax_t));
			WriteProcessMemory(process, page_path, path, size, &sent);
			if (sent == size) {
				const HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE) load_library_address, page_path, 0, NULL);
				if (thread) {
					CloseHandle(thread);
					return page;
				}
			}
		}
	}

	SHOW_ERROR(inject_alloc);
	if (page) {
		VirtualFreeEx(process, page, 0, MEM_DECOMMIT | MEM_RELEASE);
	}
	return NULL;
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

uintmax_t find_export_base_address(const HANDLE process, const char *const module) {
	char data[DATA_LENGTH] = {};

	MEMORY_BASIC_INFORMATION memory = {};
	uintmax_t query_address = 0;
	while (VirtualQueryEx(process, (void *) query_address, &memory, sizeof(MEMORY_BASIC_INFORMATION))) {
		query_address = (uintmax_t) memory.BaseAddress;

		const uint16_t stub = REMOTE_READ_WORD(process, query_address);
		if (stub == 0x5a4d) {
			const uint32_t signature_offset = REMOTE_READ_DWORD(process, query_address + 0x3c);
			const uintmax_t signature_address = query_address + signature_offset;
			if (REMOTE_READ_DWORD(process, signature_address) == 0x4550) {
				const uintmax_t optional_header_address = signature_address + 0x18;
				const uint16_t optional_magic = REMOTE_READ_WORD(process, optional_header_address);
				const uint8_t data_directories_offset = optional_magic == 0x10b ? 0x5c : 0x6c;
				const uint32_t data_directories = REMOTE_READ_DWORD(process, optional_header_address + data_directories_offset);
				if (data_directories > 0) {
					const uint32_t export_directory_table_offset = REMOTE_READ_DWORD(process, optional_header_address + data_directories_offset + 4);
					const uint32_t export_directory_table_size = REMOTE_READ_DWORD(process, optional_header_address + data_directories_offset + 8);
					if (export_directory_table_offset > 0 && export_directory_table_size > 0) {
						const uintmax_t export_directory_table_address = query_address + export_directory_table_offset;
						const uintmax_t module_name_address = query_address + REMOTE_READ_DWORD(process, export_directory_table_address + 0xc);
						data[remote_read_string(process, module_name_address, data, DATA_READ)] = 0;
						if (!stricmp(data, module)) {
							return query_address;
						}
					}
				}
			}
		}

		query_address += memory.RegionSize;
	}

	return 0;
}

uintmax_t get_entry_point(const HANDLE process) {
	MEMORY_BASIC_INFORMATION memory = {};
	uintmax_t query_address = 0;
	while (VirtualQueryEx(process, (void *) query_address, &memory, sizeof(MEMORY_BASIC_INFORMATION))) {
		query_address = (uintmax_t) memory.BaseAddress;

		const uint16_t stub = REMOTE_READ_WORD(process, query_address);
		if (stub == 0x5a4d) {
			const uint32_t signature_offset = REMOTE_READ_DWORD(process, query_address + 0x3c);
			const uintmax_t signature_address = query_address + signature_offset;
			if (REMOTE_READ_DWORD(process, signature_address) == 0x4550) {
				const uintmax_t optional_header_address = signature_address + 0x18;
				const uint16_t optional_magic = REMOTE_READ_WORD(process, optional_header_address);
				if (optional_magic == 0x10b || optional_magic == 0x20b) {
					return query_address + REMOTE_READ_DWORD(process, optional_header_address + 0x10);
				}
			}
		}

		query_address += memory.RegionSize;
	}

	return 0;
}

#define CONNECT_CODE_LENGTH 479
static uint8_t connect_code[CONNECT_CODE_LENGTH] = {
	// push rax
	0x50,
	// push rbx
	0x53,
	// push r9
	0x41, 0x51,
	// mov r9,<ipv4_patch>
	0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// cmp r8,10
	0x49, 0x83, 0xF8, 0x10,
	// jz <patch>
	0x74, 0x15,
	// mov r9,<ipv6_patch>
	0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// cmp r8,1C
	0x49, 0x83, 0xF8, 0x1C,
	// jz <patch>
	0x74, 0x05,
	// jmp <original_function>
	0xE9, 0xB3, 0x00, 0x00, 0x00,

// patch:
	// push rcx
	0x51,
	// push rdx
	0x52,
	// push r8
	0x41, 0x50,
	// push r9
	0x41, 0x51,
	// sub rsp,70
	0x48, 0x83, 0xEC, 0x70,
	// mov rcx,<mutex>
	0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// xor rdx,rdx
	0x48, 0x31, 0xD2,
	// not rdx
	0x48, 0xF7, 0xD2,
	// xor r8,r8
	0x4D, 0x31, 0xC0,
	// mov rax,<kernelbase.WaitForSingleObjectEx>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// call rax
	0xFF, 0xD0,
	// add rsp,70
	0x48, 0x83, 0xC4, 0x70,
	// pop r9
	0x41, 0x59,
	// pop r8
	0x41, 0x58,
	// pop rdx
	0x5A,
	// pop rcx
	0x59,

	// mov rbx,<data_page>
	0x48, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	// mov rax,qword ptr ss:[rsp+18]				; return address
	0x48, 0x8B, 0x44, 0x24, 0x18,
	// mov qword ptr ds:[rbx],rax
	0x48, 0x89, 0x03,
	// add rbx,8
	0x48, 0x83, 0xC3, 0x08,
	// mov rax,<call_send>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// mov qword ptr ss:[rsp+18],rax
	0x48, 0x89, 0x44, 0x24, 0x18,

	// mov qword ptr ds:[rbx],rcx					; socket
	0x48, 0x89, 0x0B,
	// add rbx,8
	0x48, 0x83, 0xC3, 0x08,

	// mov ax,word ptr ds:[rdx+2]					; port
	0x66, 0x8B, 0x42, 0x02,
	// mov word ptr ds:[rbx],ax
	0x66, 0x89, 0x03,
	// add rbx,2
	0x48, 0x83, 0xC3, 0x02,
	// mov word ptr ds:[rdx+2],2A23
	0x66, 0xC7, 0x42, 0x02, 0x23, 0x2A,

	// jmp r9							; either <ipv4_patch> or <ipv6_patch>
	0x41, 0xFF, 0xE1,

// ipv4_patch:
	// mov byte ptr ds:[rbx],0
	0xC6, 0x03, 0x00,
	// inc rbx
	0x48, 0xFF, 0xC3,

	// mov eax,dword ptr ds:[rdx+4]					; IPv4 address
	0x8B, 0x42, 0x04,
	// mov dword ptr ds:[rbx],eax
	0x89, 0x03,
	// mov dword ptr ds:[rdx+4],100007F
	0xC7, 0x42, 0x04, 0x7F, 0x00, 0x00, 0x01,
	// jmp <original_function>
	0xEB, 0x2C,

// ipv6_patch:
	// mov byte ptr ds:[rbx],1
	0xC6, 0x03, 0x01,
	// inc rbx
	0x48, 0xFF, 0xC3,

	// mov rax,qword ptr ds:[rdx+8]					; IPv6 address
	0x48, 0x8B, 0x42, 0x08,
	// mov qword ptr ds:[rbx],rax
	0x48, 0x89, 0x03,
	// add rbx,8
	0x48, 0x83, 0xC3, 0x08,
	// mov rax,qword ptr ds:[rdx+10]
	0x48, 0x8B, 0x42, 0x10,
	// mov qword ptr ds:[rbx],rax
	0x48, 0x89, 0x03,

	// mov qword ptr ds:[rdx+8],0
	0x48, 0xC7, 0x42, 0x08, 0x00, 0x00, 0x00, 0x00,
	// mov qword ptr ds:[rdx+10],0
	0x48, 0xC7, 0x42, 0x10, 0x00, 0x00, 0x00, 0x00,
	// mov byte ptr ds:[rdx+17],1
	0xC6, 0x42, 0x17, 0x01,

// original_function:
	// pop r9
	0x41, 0x59,
	// pop rbx
	0x5B,
	// pop rax
	0x58,

	// mov rax,rsp
	0x48, 0x89, 0xE0,
	// mov qword ptr ds:[rax+8],rbx
	0x48, 0x89, 0x58, 0x08,
	// mov qword ptr ds:[rax+10],rbp
	0x48, 0x89, 0x68, 0x10,
	// mov qword ptr ds:[rax+18],rsi
	0x48, 0x89, 0x70, 0x18,
	// mov rax,<ws2_32.connect+C>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// jmp rax
	0xFF, 0xE0,

// call_send:
	// sub rsp,8
	0x48, 0x83, 0xEC, 0x08,
	// cmp rax,0
	0x48, 0x83, 0xF8, 0x00,
	// jz <call_send_no_error>
	0x74, 0x1A,

// call_send_error:
	// push rax
	0x50,

	// mov rax,<data_page>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// mov rax,qword ptr ds:[rax]
	0x48, 0x8B, 0x00,
	// mov qword ptr ss:[rsp+8],rax
	0x48, 0x89, 0x44, 0x24, 0x08,

	// call <release_mutex>
	0xE8, 0x96, 0x00, 0x00, 0x00,

	// pop rax
	0x58,
	// ret
	0xC3,

// call_send_no_error:
	// push rbx
	0x53,
	// push rcx
	0x51,
	// push rdx
	0x52,
	// push r8
	0x41, 0x50,
	// push r9
	0x41, 0x51,

	// mov rbx,<data_page>
	0x48, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// mov rax,qword ptr ds:[rbx]
	0x48, 0x8B, 0x03,
	// mov qword ptr ss:[rsp+28],rax
	0x48, 0x89, 0x44, 0x24, 0x28,

	// mov rcx,qword ptr ds:[rbx+8]					; send type
	0x48, 0x8B, 0x4B, 0x08,
	// mov rdx,rbx
	0x48, 0x89, 0xDA,
	// add rdx,12
	0x48, 0x83, 0xC2, 0x12,
	// mov r8,1
	0x49, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,
	// mov rax,<dll.send_all>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// call rax
	0xFF, 0xD0,

	// mov rcx,qword ptr ds:[rbx+8]					; send IP address
	0x48, 0x8B, 0x4B, 0x08,
	// mov rdx,rbx
	0x48, 0x89, 0xDA,
	// add rdx,12
	0x48, 0x83, 0xC2, 0x12,

	// cmp byte ptr ds:[rdx],1
	0x80, 0x3A, 0x01,
	// jz <call_send_ipv6>
	0x74, 0x09,
	// mov r8,4
	0x49, 0xC7, 0xC0, 0x04, 0x00, 0x00, 0x00,
	// jmp <call_send_ipv4>
	0xEB, 0x07,

// call_send_ipv6:
	// mov r8,10
	0x49, 0xC7, 0xC0, 0x10, 0x00, 0x00, 0x00,

// call_send_ipv4:
	// inc rdx
	0x48, 0xFF, 0xC2,
	// mov rax,<dll.send_all>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// call rax
	0xFF, 0xD0,

	// mov rcx,qword ptr ds:[rbx+8]					; send port
	0x48, 0x8B, 0x4B, 0x08,
	// mov rdx,rbx
	0x48, 0x89, 0xDA,
	// add rdx,10
	0x48, 0x83, 0xC2, 0x10,
	// mov r8,2
	0x49, 0xC7, 0xC0, 0x02, 0x00, 0x00, 0x00,
	// mov rax,<dll.send_all>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// call rax
	0xFF, 0xD0,

	// call <release_mutex>
	0xE8, 0x0B, 0x00, 0x00, 0x00,

	// pop r9
	0x41, 0x59,
	// pop r8
	0x41, 0x58,
	// pop rdx
	0x5A,
	// pop rcx
	0x59,
	// pop rbx
	0x5B,

	// xor rax,rax
	0x48, 0x31, 0xC0,
	// ret
	0xC3,

// release_mutex:
	// push rcx
	0x51,
	// push rdx
	0x52,
	// push r8
	0x41, 0x50,
	// push r9
	0x41, 0x51,
	// mov rcx,<mutex>
	0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// mov rax,<kernelbase.ReleaseMutex>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// call rax
	0xFF, 0xD0,
	// pop r9
	0x41, 0x59,
	// pop r8
	0x41, 0x58,
	// pop rdx
	0x5A,
	// pop rcx
	0x59,
	// ret
	0xC3,
};

#define HOOK_CODE_OFFSET 2
#define HOOK_CODE_LENGTH 15
static uint8_t hook_code[HOOK_CODE_LENGTH] = {
	// mov rax,<code_page>
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// jmp rax
	0xFF, 0xE0,
	// mov rax,rsp
	0x48, 0x8B, 0xC4,
};

void *patch_connect(const HANDLE process, const uintmax_t connect_address, const uintmax_t release_address, const uintmax_t wait_address) {
	void *const code_page = VirtualAllocEx(process, NULL, DEFAULT_PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!code_page) {
		return NULL;
	}

	void *const data_page = VirtualAllocEx(process, NULL, DEFAULT_PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!data_page) {
		VirtualFreeEx(process, code_page, 0, MEM_DECOMMIT | MEM_RELEASE);
		return NULL;
	}

	*((uint64_t *) (hook_code + HOOK_CODE_OFFSET)) = (uint64_t) code_page;

	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_1)) = (uint64_t) code_page + CONNECT_CODE_IPV4_PATCH_OFFSET;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_2)) = (uint64_t) code_page + CONNECT_CODE_IPV6_PATCH_OFFSET;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_4)) = wait_address;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_5)) = (uint64_t) data_page;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_6)) = (uint64_t) code_page + CONNECT_CODE_CALL_SEND_OFFSET;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_7)) = connect_address + 0x0C;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_8)) = (uint64_t) data_page;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_9)) = (uint64_t) data_page;
	*((uint64_t *) (connect_code + CONNECT_CODE_OFFSET_14)) = release_address;

	MEMORY_BASIC_INFORMATION memory = {};
	VirtualQueryEx(process, (LPCVOID) connect_address, &memory, sizeof(MEMORY_BASIC_INFORMATION));
	void *const query_address = memory.BaseAddress;
	const size_t query_size = memory.RegionSize;

	DWORD protect = 0;
	VirtualProtectEx(process, query_address, query_size, PAGE_READWRITE, &protect);

	size_t sent = 0;
	WriteProcessMemory(process, (void *) connect_address, hook_code, HOOK_CODE_LENGTH, &sent);
	if (sent != HOOK_CODE_LENGTH) {
		SHOW_ERROR(WriteProcessMemory);
	}
	WriteProcessMemory(process, code_page, connect_code, CONNECT_CODE_LENGTH, &sent);
	if (sent != CONNECT_CODE_LENGTH) {
		SHOW_ERROR(WriteProcessMemory);
	}
	VirtualProtectEx(process, query_address, query_size, protect, &protect);

	VirtualProtectEx(process, code_page, DEFAULT_PAGE_SIZE, PAGE_EXECUTE_READ, &protect);

	return code_page;
}

char *get_basename(const char *const path) {
	size_t size = strlen(path);
	if (size > 0) {
		const char value = path[--size];
		if (value == '/' || value == '\\') {
			return (char *) (path + size + 1);
		}

		while (--size > 0) {
			const char value = path[size];
			if (value == '/' || value == '\\') {
				return (char *) (path + size + 1);
			}
		}

		return (char *) path;
	}

	return NULL;
}

HANDLE start_inject_process(const char *const executable, const char *const dll_path, size_t *const identifier) {
	STARTUPINFOA startup = {};
	PROCESS_INFORMATION info = {};

	size_t size = strlen(dll_path);
	if (size < 1) {
		return NULL;
	}

	size = strlen(executable);
	if (size < 1) {
		return NULL;
	}

	char *const working_directory = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, size + 1);
	if (!working_directory) {
		return NULL;
	}
	memcpy(working_directory, executable, size);

	while (size > 0) {
		const char value = working_directory[--size];
		if (value == '/' || value == '\\') {
			working_directory[size] = 0;
			break;
		}
	}

	startup.cb = sizeof(STARTUPINFOA);
	if (!CreateProcessA(executable, NULL, NULL, NULL, 0, CREATE_SUSPENDED, NULL, working_directory, &startup, &info)) {
		SHOW_ERROR(CreateProcessA);
		GlobalFree(working_directory);
		return NULL;
	}

	const uintmax_t entry_point = get_entry_point(info.hProcess);
	uint16_t value = 0;
	const uint16_t spin = 0xfeeb;
	size = 0;
	ReadProcessMemory(info.hProcess, (void *) entry_point, &value, sizeof(uint16_t), &size);
	if (size != sizeof(uint16_t)) {
		SHOW_ERROR(ReadProcessMemory);
		goto error_terminate;
	}

	WriteProcessMemory(info.hProcess, (void *) entry_point, &spin, sizeof(uint16_t), &size);
	if (size != sizeof(uint16_t)) {
		SHOW_ERROR(WriteProcessMemory);
		goto error_terminate;
	}

	ResumeThread(info.hThread);
	uintmax_t kernelbase = find_export_base_address(info.hProcess, "kernelbase.dll");
	while (!kernelbase) {
		kernelbase = find_export_base_address(info.hProcess, "kernelbase.dll");
	}

	const uintmax_t load_library_address = find_export_function(info.hProcess, kernelbase, "LoadLibraryA");
	const uintmax_t release_address = find_export_function(info.hProcess, kernelbase, "ReleaseMutex");
	const uintmax_t wait_address = find_export_function(info.hProcess, kernelbase, "WaitForSingleObjectEx");

	uintmax_t ws2_32 = find_export_base_address(info.hProcess, "ws2_32.dll");
	while (!ws2_32) {
		ws2_32 = find_export_base_address(info.hProcess, "ws2_32.dll");
	}

	SuspendThread(info.hThread);
	WriteProcessMemory(info.hProcess, (void *) entry_point, &value, sizeof(uint16_t), &size);
	if (size != sizeof(uint16_t)) {
		SHOW_ERROR(WriteProcessMemory);
		goto error_terminate;
	}

	const uintmax_t connect_address = find_export_function(info.hProcess, ws2_32, "connect");
	void *const code_page = patch_connect(info.hProcess, connect_address, release_address, wait_address);
	if (!code_page) {
		SHOW_ERROR(patch_connect);
		goto error_terminate;
	}
	void *const dll_data_page = inject_alloc(info.hProcess, dll_path, strlen(dll_path) + 1, (uintmax_t) code_page, load_library_address);
	if (!dll_data_page) {
		SHOW_ERROR(inject_alloc);
		goto error_terminate;
	}

	const char *const basename = get_basename(dll_path);
	uintmax_t dll_address = find_export_base_address(info.hProcess, basename);
	while (!dll_address) {
		dll_address = find_export_base_address(info.hProcess, basename);
	}
	const uintmax_t initialize_address = find_export_function(info.hProcess, dll_address, "initialize");
	const HANDLE thread = CreateRemoteThread(info.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE) initialize_address, dll_data_page, 0, NULL);
	if (thread) {
		CloseHandle(thread);
	} else {
		SHOW_ERROR(CreateRemoteThread);
		goto error_terminate;
	}

	ResumeThread(info.hThread);
	CloseHandle(info.hThread);

	GlobalFree(working_directory);
	if (identifier) {
		*identifier = info.dwProcessId;
	}
	return info.hProcess;

error_terminate:
	TerminateProcess(info.hProcess, 0);
	ResumeThread(info.hThread);
	CloseHandle(info.hProcess);
	CloseHandle(info.hThread);

	GlobalFree(working_directory);
	return NULL;
}
