#include "proxy.h"
#include "starter.h"
#include <string.h>
#include "window.h"

static lines_t lines = {};
static window_t window = {};

static HBRUSH black_brush = NULL;
static HBRUSH blue_brush = NULL;
static HBRUSH pink_brush = NULL;
static HBRUSH white_brush = NULL;

void set_lines_total_dimensions(lines_t *const lines) {
	lines->total_width = lines->width + 2 * DRAW_BORDER_WIDTH + SCROLLBAR_WIDTH;
	lines->total_height = lines->height + 2 * DRAW_BORDER_WIDTH;
}

void invalidate_lines(const HWND hwnd, const lines_t *const lines) {
	const size_t left = lines->x;
	const size_t top = lines->y;
	RECT rect = {left - DRAW_BORDER_WIDTH, top - DRAW_BORDER_WIDTH, left + lines->width + DRAW_BORDER_WIDTH + SCROLLBAR_WIDTH, top + lines->height + DRAW_BORDER_WIDTH};
	InvalidateRect(hwnd, &rect, 0);
}

void reset_lines(const HWND hwnd, lines_t *const lines) {
	lines->index = lines->count = 0;
	lines->scroll_value = lines->max_scroll_value = 0;

	invalidate_lines(hwnd, lines);
}

void resize_lines(const HWND hwnd, lines_t *const lines, const size_t width, const size_t height) {
	lines->width = width;
	lines->height = height;
	set_lines_total_dimensions(lines);

	const size_t total_line_height = lines->count * DRAW_LINE_HEIGHT;
	if (total_line_height > height) {
		lines->max_scroll_value = total_line_height - height;

		const size_t max_scroll_value = lines->max_scroll_value;
		if (lines->scroll_value > max_scroll_value) {
			lines->scroll_value = max_scroll_value;
		}
	} else {
		lines->scroll_value = lines->max_scroll_value = 0;
	}

	DeleteObject(lines->bitmap);
	lines->bitmap = CreateCompatibleBitmap(GetDC(hwnd), lines->total_width, lines->total_height);
	invalidate_lines(hwnd, lines);
}

void add_line(lines_t *const lines, const char *const line) {
	const size_t index = lines->index;
	if (index < MAX_LINES) {
		snprintf(lines->lines + index * LINE_LENGTH, LINE_LENGTH, "%s", line);
		++lines->index;
	} else {
		snprintf(lines->lines, LINE_LENGTH, "%s", line);
		lines->index = 1;
	}

	if (lines->count < MAX_LINES) {
		++lines->count;
	}

	const size_t height = lines->height;
	const size_t total_line_height = lines->count * DRAW_LINE_HEIGHT;
	if (total_line_height > height) {
		lines->max_scroll_value = total_line_height - height;
	}
}

void window_add_to_file(window_t *const window, const char *const line) {
	const HANDLE mutex = window->file_mutex;
	FILE *const stream = window->file;
	WaitForSingleObjectEx(mutex, INFINITE, 0);
	fprintf(stream, "%s\n", line);
	fflush(stream);
	ReleaseMutex(mutex);
}

void window_add_to_lines(window_t *const window, const char *const line) {
	const HANDLE mutex = window->lines_mutex;
	lines_t *const lines = window->lines;
	WaitForSingleObjectEx(mutex, INFINITE, 0);
	add_line(lines, line);
	invalidate_lines(window->hwnd, lines);
	ReleaseMutex(mutex);
}

void window_add_name_to_lines(window_t *const window, const char *const name) {
	char line[LINE_LENGTH] = {};
	snprintf(line, LINE_LENGTH, "File: %s", name);
	window_add_to_lines(window, line);
	window_add_to_lines(window, "");
}

void scroll_delta(const HWND hwnd, lines_t *const lines, const intmax_t delta) {
	const size_t height = lines->height;
	const size_t total_line_height = lines->count * DRAW_LINE_HEIGHT;
	if (total_line_height > height) {
		const size_t max_scroll_value = lines->max_scroll_value;

		size_t bar_height = (size_t) ((double) lines->total_height * (double) height / (double) total_line_height);
		if (bar_height < MINIMUM_SCROLLBAR_HEIGHT) {
			bar_height = MINIMUM_SCROLLBAR_HEIGHT;
		}

		const double new_delta = (double) delta * (double) max_scroll_value / (double) (lines->total_height - bar_height);
		const intmax_t value = lines->scroll_value + (intmax_t) new_delta;
		if (value > (intmax_t) max_scroll_value) {
			lines->scroll_value = max_scroll_value;
		} else if (value < 0) {
			lines->scroll_value = 0;
		} else {
			lines->scroll_value = value;
		}

		invalidate_lines(hwnd, lines);
	}
}

void scroll_delta_lines(const HWND hwnd, lines_t *const lines, const intmax_t delta) {
	const size_t height = lines->height;
	const size_t total_line_height = lines->count * DRAW_LINE_HEIGHT;
	if (total_line_height > height) {
		const size_t max_scroll_value = lines->max_scroll_value;

		const intmax_t value = lines->scroll_value + delta * DRAW_LINE_HEIGHT;
		if (value > (intmax_t) max_scroll_value) {
			lines->scroll_value = max_scroll_value;
		} else if (value < 0) {
			lines->scroll_value = 0;
		} else {
			lines->scroll_value = value;
		}

		invalidate_lines(hwnd, lines);
	}
}

void paint_scrollbar(const HDC hdc, const lines_t *const lines) {
	const size_t height = lines->height;
	const size_t total_line_height = lines->count * DRAW_LINE_HEIGHT;
	if (total_line_height > height) {
		size_t bar_height = (size_t) ((double) lines->total_height * (double) height / (double) total_line_height);
		if (bar_height < MINIMUM_SCROLLBAR_HEIGHT) {
			bar_height = MINIMUM_SCROLLBAR_HEIGHT;
		}

		const size_t left = lines->width + 2 * DRAW_BORDER_WIDTH;
		const size_t top = (size_t) ((double) (lines->total_height - bar_height) * ((double) lines->scroll_value / (double) lines->max_scroll_value));

		const double bar_height_delta = (double) bar_height / 5.0;
		RECT rect = {left, top, left + SCROLLBAR_WIDTH, top + (LONG) bar_height_delta};
		FillRect(hdc, &rect, blue_brush);

		rect.top = rect.bottom;
		rect.bottom = top + (LONG) (2.0 * bar_height_delta);
		FillRect(hdc, &rect, pink_brush);

		rect.top = rect.bottom;
		rect.bottom = top + (LONG) (3.0 * bar_height_delta);
		FillRect(hdc, &rect, white_brush);

		rect.top = rect.bottom;
		rect.bottom = top + (LONG) (4.0 * bar_height_delta);
		FillRect(hdc, &rect, pink_brush);

		rect.top = rect.bottom;
		rect.bottom = top + bar_height;
		FillRect(hdc, &rect, blue_brush);
	}
}

#define DRAW_LINE(hdc, lines, index, rect)							\
	DrawTextA(hdc, lines + (index) * LINE_LENGTH, -1, &rect, DT_LEFT | DT_SINGLELINE);	\
	rect.top += DRAW_LINE_HEIGHT;								\
	rect.bottom += DRAW_LINE_HEIGHT								\

void paint_lines(const HWND hwnd, const lines_t *const lines) {
	PAINTSTRUCT paint = {};
	const HDC hdc = BeginPaint(hwnd, &paint);
	const HDC source = CreateCompatibleDC(hdc);
	if (!source) {
		return;
	}
	SelectObject(source, lines->bitmap);

	const size_t total_width = lines->total_width;
	const size_t total_height = lines->total_height;
	RECT rect = {0, 0, total_width, total_height};
	FillRect(source, &rect, white_brush);

	const char *const data = lines->lines;
	const size_t index = lines->index;
	const size_t count = lines->count;

	const size_t width = lines->width;
	const size_t height = lines->height;
	const size_t max_y = height + DRAW_BORDER_WIDTH;
	rect.left = DRAW_BORDER_WIDTH;
	rect.top = DRAW_BORDER_WIDTH;
	rect.right = width + DRAW_BORDER_WIDTH;
	rect.bottom = max_y;
	FillRect(source, &rect, pink_brush);

	const size_t scroll_value = lines->scroll_value;
	rect.top -= scroll_value;
	rect.bottom = DRAW_BORDER_WIDTH + DRAW_LINE_HEIGHT - scroll_value;
	size_t i = 0;
	SetBkMode(source, TRANSPARENT);
	if (count < MAX_LINES) {
		if (rect.top < DRAW_LINE_MINIMUM_Y) {
			const size_t j = (DRAW_LINE_MINIMUM_Y - rect.top) / DRAW_LINE_HEIGHT;
			const size_t offset = j * DRAW_LINE_HEIGHT;
			i += j;
			rect.top += offset;
			rect.bottom += offset;
		}

		while (i < count) {
			if (rect.top < DRAW_LINE_MINIMUM_Y) {
				++i;
				rect.top += DRAW_LINE_HEIGHT;
				rect.bottom += DRAW_LINE_HEIGHT;
				continue;
			}

			DRAW_LINE(source, data, i++, rect);
			if ((size_t) rect.top > max_y) {
				break;
			}
		}
	} else {
		if (index < MAX_LINES) {
			i = index;
		}
		const size_t j = i;

		if (rect.top < DRAW_LINE_MINIMUM_Y) {
			const size_t j = (DRAW_LINE_MINIMUM_Y - rect.top) / DRAW_LINE_HEIGHT;
			const size_t offset = j * DRAW_LINE_HEIGHT;
			i += j;
			rect.top += offset;
			rect.bottom += offset;
		}

		while (i < count) {
			if (rect.top < DRAW_LINE_MINIMUM_Y) {
				++i;
				rect.top += DRAW_LINE_HEIGHT;
				rect.bottom += DRAW_LINE_HEIGHT;
				continue;
			}

			DRAW_LINE(source, data, i++, rect);
			if ((size_t) rect.top > max_y) {
				break;
			}
		}

		if (i > count) {
			i -= count;
		} else {
			i = 0;
		}
		while (i < j) {
			if (rect.top < DRAW_LINE_MINIMUM_Y) {
				++i;
				rect.top += DRAW_LINE_HEIGHT;
				rect.bottom += DRAW_LINE_HEIGHT;
				continue;
			}

			DRAW_LINE(source, data, i++, rect);
			if ((size_t) rect.top > max_y) {
				break;
			}
		}
	}
	paint_scrollbar(source, lines);

	rect.left = rect.top = 0;
	rect.right = DRAW_BORDER_WIDTH;
	rect.bottom = total_height;
	FillRect(source, &rect, blue_brush);

	rect.left = DRAW_BORDER_WIDTH;
	rect.top = total_height - DRAW_BORDER_WIDTH;
	rect.right = width + DRAW_BORDER_WIDTH;
	rect.bottom = total_height;
	FillRect(source, &rect, blue_brush);

	rect.left = width + DRAW_BORDER_WIDTH;
	rect.top = 0;
	rect.right = width + 2 * DRAW_BORDER_WIDTH;
	rect.bottom = total_height;
	FillRect(source, &rect, blue_brush);

	rect.left = DRAW_BORDER_WIDTH;
	rect.top = 0;
	rect.right = width + DRAW_BORDER_WIDTH;
	rect.bottom = DRAW_BORDER_WIDTH;
	FillRect(source, &rect, blue_brush);

	BitBlt(hdc, lines->x - DRAW_BORDER_WIDTH, lines->y - DRAW_BORDER_WIDTH, total_width, total_height, source, 0, 0, SRCCOPY);
	DeleteObject(source);

	EndPaint(hwnd, &paint);
}

void resize_all(const window_t *const window, const size_t width, const size_t height) {
	MoveWindow(window->hwnd_pause_resume, WINDOW_MARGIN, FIX_WINDOW_Y(WINDOW_MARGIN), TOP_BUTTON_WIDTH(width), BUTTON_HEIGHT, 1);
	MoveWindow(window->hwnd_save_snapshot, 2 * WINDOW_MARGIN + TOP_BUTTON_WIDTH(width), FIX_WINDOW_Y(WINDOW_MARGIN), TOP_BUTTON_WIDTH(width), BUTTON_HEIGHT, 1);

	resize_lines(window->hwnd, window->lines, width - 2 * WINDOW_MARGIN - 2 * DRAW_BORDER_WIDTH - SCROLLBAR_WIDTH, height - 5 * WINDOW_MARGIN - 2 * DRAW_BORDER_WIDTH - 3 * BUTTON_HEIGHT);

	MoveWindow(window->hwnd_dll_path, WINDOW_MARGIN, FIX_WINDOW_Y(height - 2 * (WINDOW_MARGIN + BUTTON_HEIGHT)), width - 2 * WINDOW_MARGIN, BUTTON_HEIGHT, 1);
	MoveWindow(window->hwnd_executable_path, WINDOW_MARGIN, BOTTOM_Y(height), EXECUTABLE_PATH_WIDTH(width), BUTTON_HEIGHT, 1);
	MoveWindow(window->hwnd_start, EXECUTABLE_PATH_WIDTH(width) + 2 * WINDOW_MARGIN, BOTTOM_Y(height), START_BUTTON_WIDTH, BUTTON_HEIGHT, 1);
	MoveWindow(window->hwnd_create_file, EXECUTABLE_PATH_WIDTH(width) + START_BUTTON_WIDTH + 3 * WINDOW_MARGIN, BOTTOM_Y(height), CHECKBOX_WIDTH, BUTTON_HEIGHT, 1);
}

FILE *create_packet_log(char *const name, const size_t size) {
	SYSTEMTIME time = {};

	GetLocalTime(&time);
	snprintf(name, size, "%04d-%02d-%02d_%02d_%02d_%02d_packet_log.txt", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	FILE *const stream = fopen(name, "r");
	if (stream) {
		SHOW_ERROR("File exists!");
		fclose(stream);
		return NULL;
	}

	return fopen(name, "w");
}

void save_snapshot(const lines_t *const lines) {
	char data[DATA_LENGTH] = {};
	FILE *const stream = create_packet_log(data, DATA_LENGTH);
	if (!stream) {
		return;
	}

	const char *const value = lines->lines;
	const size_t index = lines->index;
	const size_t count = lines->count;

	size_t i = 0;
	if (count < MAX_LINES) {
		while (i < count) {
			fprintf(stream, "%s\n", value + (i++) * LINE_LENGTH);
		}
	} else {
		if (index < MAX_LINES) {
			i = index;
		}
		const size_t j = i;

		while (i < count) {
			fprintf(stream, "%s\n", value + (i++) * LINE_LENGTH);
		}

		i = 0;
		while (i < j) {
			fprintf(stream, "%s\n", value + (i++) * LINE_LENGTH);
		}
	}

	fclose(stream);
}

FILE *window_create_file(window_t *const window) {
	char data[DATA_LENGTH] = {};
	FILE *const stream = create_packet_log(data, DATA_LENGTH);
	if (!stream) {
		window->file = NULL;
		return NULL;
	}
	window_add_name_to_lines(window, data);

	window->file = stream;
	return stream;
}

void window_handle_button(window_t *const window, const HWND hwnd) {
	if (hwnd == window->hwnd_start) {
		EnableWindow(window->hwnd_dll_path, 0);
		EnableWindow(window->hwnd_executable_path, 0);
		EnableWindow(window->hwnd_start, 0);
		EnableWindow(window->hwnd_create_file, 0);
		reset_lines(window->hwnd, window->lines);

		const HANDLE mutex = window->started_mutex;
		WaitForSingleObjectEx(mutex, INFINITE, 0);
		if (!window->is_started) {
			FILE *stream = NULL;
			uint8_t error = 0;

			const LRESULT result = SendMessage(window->hwnd_create_file, BM_GETCHECK, 0, 0);
			if (result == BST_CHECKED) {
				stream = window_create_file(window);
				if (!stream) {
					error = 1;
				}
				window->is_adding_to_file = 1;
			} else {
				window->is_adding_to_file = 0;
			}

			if (!error) {
				GetWindowTextA(window->hwnd_dll_path, window->dll_path, MAX_PATH);
				GetWindowTextA(window->hwnd_executable_path, window->executable_path, MAX_PATH);
				error = start_inject_process(window->executable_path, window->dll_path);
			}

			if (error) {
				if (stream) {
					fclose(stream);
				}
			} else {
				window->is_started = 1;
			}
		}

		if (window->is_started) {
			EnableWindow(window->hwnd_pause_resume, 1);
			EnableWindow(window->hwnd_save_snapshot, 1);
		} else {
			EnableWindow(window->hwnd_dll_path, 1);
			EnableWindow(window->hwnd_executable_path, 1);
			EnableWindow(window->hwnd_start, 1);
			EnableWindow(window->hwnd_create_file, 1);
		}
		ReleaseMutex(mutex);
	} else if (hwnd == window->hwnd_pause_resume) {
		window->is_adding_to_lines = !window->is_adding_to_lines;
		if (window->is_adding_to_lines) {
			SetWindowTextA(window->hwnd_pause_resume, "Pause capture");
		} else {
			SetWindowTextA(window->hwnd_pause_resume, "Resume capture");
		}
	} else if (hwnd == window->hwnd_save_snapshot) {
		save_snapshot(window->lines);
	}
}

#define GET_CURSOR_X(lParam) (lParam & 65535)
#define GET_CURSOR_Y(lParam) ((lParam >> 16) & 65535)

LRESULT window_procedure(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			window_handle_button(&window, (HWND) lParam);
			break;
		case WM_KEYDOWN:
			switch (wParam) {
				case VK_DOWN:
					scroll_delta_lines(hwnd, &lines, 1);
					break;
				case VK_UP:
					scroll_delta_lines(hwnd, &lines, -1);
					break;
			}
			break;
		case WM_LBUTTONDOWN:
			if ((wParam & MK_LBUTTON) == MK_LBUTTON) {
				const size_t height = lines.height;
				const size_t total_line_height = lines.count * DRAW_LINE_HEIGHT;
				if (total_line_height > height) {
					size_t bar_height = (size_t) ((double) lines.total_height * (double) height / (double) total_line_height);
					if (bar_height < MINIMUM_SCROLLBAR_HEIGHT) {
						bar_height = MINIMUM_SCROLLBAR_HEIGHT;
					}

					const size_t offset = (size_t) ((double) (lines.total_height - bar_height) * ((double) lines.scroll_value / (double) lines.max_scroll_value));
					const size_t left = lines.x + lines.width + DRAW_BORDER_WIDTH;
					const size_t top = lines.y - DRAW_BORDER_WIDTH + offset;

					const uint16_t x = GET_CURSOR_X(lParam);
					const uint16_t y = GET_CURSOR_Y(lParam);
					if (left <= x && x < (left + SCROLLBAR_WIDTH) && top <= y && y < (top + bar_height)) {
						window.mouse_down = 1;
						window.mouse_x = x;
						window.mouse_y = y;
					}
				}
			}
			break;
		case WM_LBUTTONUP:
			window.mouse_down = 0;
			window.mouse_x = 0;
			window.mouse_y = 0;
			break;
		case WM_MOUSEMOVE:
			if (window.mouse_down && (wParam & MK_LBUTTON) == MK_LBUTTON) {
				const uint16_t x = GET_CURSOR_X(lParam);
				const uint16_t y = GET_CURSOR_Y(lParam);
				scroll_delta(hwnd, &lines, y - window.mouse_y);

				window.mouse_x = x;
				window.mouse_y = y;
			}
			break;
		case WM_PAINT:
			paint_lines(hwnd, &lines);
			break;
		case WM_SIZE:
			resize_all(&window, LOWORD(lParam), RESIZE_FIX_WINDOW_HEIGHT(HIWORD(lParam)));
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
	WSADATA wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		SHOW_ERROR(WSAStartup);
		return 1;
	}

	black_brush = GetStockObject(BLACK_BRUSH);
	white_brush = GetStockObject(WHITE_BRUSH);

	WNDCLASSA value = {};
	value.style = CS_HREDRAW | CS_VREDRAW;
	value.lpfnWndProc = window_procedure;
	value.cbClsExtra = 0;
	value.cbWndExtra = 0;
	value.hInstance = NULL;
	value.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	value.hCursor = LoadCursor(NULL, IDC_ARROW);
	value.hbrBackground = white_brush;
	value.lpszMenuName = NULL;
	value.lpszClassName = "PROXY";
	if (!RegisterClass(&value)) {
		SHOW_ERROR(RegisterClass);
		return 1;
	}

	char *const data = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, MAX_LINES * LINE_LENGTH);
	if (!data) {
		SHOW_ERROR(GlobalAlloc);
		return 1;
	}
	lines.lines = data;

	if (!(window.dll_path = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, MAX_PATH))) {
		SHOW_ERROR(GlobalAlloc);
		goto error_dll_path;
	}

	if (!(window.executable_path = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, MAX_PATH))) {
		SHOW_ERROR(GlobalAlloc);
		goto error_executable_path;
	}

	lines.x = WINDOW_MARGIN + DRAW_BORDER_WIDTH;
	lines.y = FIX_WINDOW_Y(2 * WINDOW_MARGIN + BUTTON_HEIGHT + DRAW_BORDER_WIDTH);

	if (!(blue_brush = CreateSolidBrush(RGB(85, 205, 252)))) {
		SHOW_ERROR(CreateSolidBrush);
		goto error_blue_brush;
	}

	if (!(pink_brush = CreateSolidBrush(RGB(247, 168, 184)))) {
		SHOW_ERROR(CreateSolidBrush);
		goto error_pink_brush;
	}

	if (!(window.hwnd = CreateWindowA(
		"PROXY",
		"Trove Proxy",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		NULL,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	const HWND hwnd = window.hwnd;
	if (!(window.hwnd_pause_resume = CreateWindowA(
		"BUTTON",
		"Pause capture",
		BS_PUSHBUTTON | WS_CHILD | WS_DISABLED | WS_VISIBLE,
		WINDOW_MARGIN,
		FIX_WINDOW_Y(WINDOW_MARGIN),
		TOP_BUTTON_WIDTH(FIX_WINDOW_WIDTH(WINDOW_WIDTH)),
		BUTTON_HEIGHT,
		hwnd,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	if (!(window.hwnd_save_snapshot = CreateWindowA(
		"BUTTON",
		"Save current snapshot",
		BS_PUSHBUTTON | WS_CHILD | WS_DISABLED | WS_VISIBLE,
		2 * WINDOW_MARGIN + TOP_BUTTON_WIDTH(FIX_WINDOW_WIDTH(WINDOW_WIDTH)),
		FIX_WINDOW_Y(WINDOW_MARGIN),
		TOP_BUTTON_WIDTH(FIX_WINDOW_WIDTH(WINDOW_WIDTH)),
		BUTTON_HEIGHT,
		hwnd,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	if (!(window.hwnd_dll_path = CreateWindowA(
		"EDIT",
		"C:\\path\\to\\dll",
		ES_AUTOHSCROLL | WS_CHILD | WS_VISIBLE,
		WINDOW_MARGIN,
		FIX_WINDOW_Y(FIX_WINDOW_HEIGHT(WINDOW_HEIGHT) - 2 * (WINDOW_MARGIN + BUTTON_HEIGHT)),
		FIX_WINDOW_WIDTH(WINDOW_WIDTH) - 2 * WINDOW_MARGIN,
		BUTTON_HEIGHT,
		hwnd,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	if (!(window.hwnd_executable_path = CreateWindowA(
		"EDIT",
		"C:\\path\\to\\exe",
		ES_AUTOHSCROLL | WS_CHILD | WS_VISIBLE,
		WINDOW_MARGIN,
		BOTTOM_Y(FIX_WINDOW_HEIGHT(WINDOW_HEIGHT)),
		EXECUTABLE_PATH_WIDTH(FIX_WINDOW_WIDTH(WINDOW_WIDTH)),
		BUTTON_HEIGHT,
		hwnd,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	if (!(window.hwnd_start = CreateWindowA(
		"BUTTON",
		"Start",
		BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
		EXECUTABLE_PATH_WIDTH(FIX_WINDOW_WIDTH(WINDOW_WIDTH)) + 2 * WINDOW_MARGIN,
		BOTTOM_Y(FIX_WINDOW_HEIGHT(WINDOW_HEIGHT)),
		START_BUTTON_WIDTH,
		BUTTON_HEIGHT,
		hwnd,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	if (!(window.hwnd_create_file = CreateWindowA(
		"BUTTON",
		"create file",
		BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE,
		EXECUTABLE_PATH_WIDTH(FIX_WINDOW_WIDTH(WINDOW_WIDTH)) + START_BUTTON_WIDTH + 3 * WINDOW_MARGIN,
		BOTTOM_Y(FIX_WINDOW_HEIGHT(WINDOW_HEIGHT)),
		CHECKBOX_WIDTH,
		BUTTON_HEIGHT,
		hwnd,
		NULL,
		NULL,
		NULL
	))) {
		SHOW_ERROR(CreateWindowA);
		goto error_hwnd;
	}

	if (!(lines.bitmap = CreateCompatibleBitmap(GetDC(hwnd), lines.total_width, lines.total_height))) {
		SHOW_ERROR(CreateCompatibleBitmap);
		goto error_hwnd;
	}

	if (!(window.file_mutex = CreateMutexA(NULL, 0, NULL))) {
		SHOW_ERROR(CreateMutexA);
		goto error_file_mutex;
	}

	if (!(window.lines_mutex = CreateMutexA(NULL, 0, NULL))) {
		SHOW_ERROR(CreateMutexA);
		goto error_lines_mutex;
	}
	window.lines = &lines;
	window.is_adding_to_lines = 1;

	if (!(window.started_mutex = CreateMutexA(NULL, 0, NULL))) {
		SHOW_ERROR(CreateMutexA);
		goto error_started_mutex;
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	const HANDLE proxy_thread = CreateThread(NULL, 0, start_proxy, (void *) &window, 0, NULL);

	MSG msg = {};
	uint8_t loop = 0;
	while ((loop = GetMessage(&msg, hwnd, 0, 0))) {
		if (loop == (uint8_t) -1) {
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (window.file) {
		fclose(window.file);
		window.file = NULL;
	}
	CloseHandle(proxy_thread);

	CloseHandle(window.file_mutex);
	CloseHandle(window.lines_mutex);
	CloseHandle(window.started_mutex);

	DeleteObject(lines.bitmap);
	DeleteObject(blue_brush);
	DeleteObject(pink_brush);

	GlobalFree(data);
	WSACleanup();
	return 0;

error_started_mutex:
	CloseHandle(window.lines_mutex);
error_lines_mutex:
	CloseHandle(window.file_mutex);
error_file_mutex:
	DeleteObject(lines.bitmap);
error_hwnd:
	DeleteObject(pink_brush);
error_pink_brush:
	DeleteObject(blue_brush);
error_blue_brush:
	GlobalFree(window.executable_path);
error_executable_path:
	GlobalFree(window.dll_path);
error_dll_path:
	GlobalFree(data);
	WSACleanup();
	return 1;
}
