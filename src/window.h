#ifndef WINDOW_H
#define WINDOW_H
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#define MAX_LINES 16384
#define LINE_LENGTH 128
#define LINE_READ (LINE_LENGTH - 1)

#define SHOW_ERROR(text) MessageBoxA(NULL, "Error: " #text, NULL, MB_ICONERROR)

typedef struct {
	char *lines;
	size_t index;
	size_t count;

	size_t x;
	size_t y;
	size_t width;
	size_t height;

	size_t total_width;
	size_t total_height;

	size_t scroll_value;
	size_t max_scroll_value;

	HFONT font;
	HBITMAP bitmap;
} lines_t;

typedef struct {
	HWND hwnd;

	HWND hwnd_pause_resume;
	HWND hwnd_clear_snapshot;
	HWND hwnd_save_snapshot;
	HWND hwnd_previous_packet;
	HWND hwnd_next_packet;

	HWND hwnd_dll_path;
	HWND hwnd_executable_path;
	HWND hwnd_start;
	HWND hwnd_create_file;

	char *dll_path;
	char *executable_path;

	HANDLE print_mutex;
	HANDLE file_mutex;
	FILE *file;
	uint8_t is_adding_to_file;

	HANDLE lines_mutex;
	lines_t *lines;
	uint8_t is_adding_to_lines;

	HANDLE started_mutex;
	uint8_t is_started;
	HANDLE process;

	uint8_t mouse_down;
	size_t mouse_x;
	size_t mouse_y;
} window_t;

#define DRAW_LINE_HEIGHT 16
#define DRAW_BORDER_WIDTH 4
#define DRAW_LINE_MINIMUM_Y (DRAW_BORDER_WIDTH - DRAW_LINE_HEIGHT)

#define MINIMUM_SCROLLBAR_HEIGHT 17
#define SCROLLBAR_WIDTH 20

#define WINDOW_MARGIN 8
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define BUTTON_HEIGHT (2 * DRAW_LINE_HEIGHT)

#define FIX_WINDOW_WIDTH(width) (width - 16)
#define FIX_WINDOW_HEIGHT(height) (height - 32)
#define FIX_WINDOW_Y(y) (y - 8)

#define RESIZE_FIX_WINDOW_HEIGHT(height) (height + 8)

#define TOP_BUTTON_WIDTH(width) ((width - 6 * WINDOW_MARGIN) / 5)
#define BOTTOM_Y(height) FIX_WINDOW_Y(height - WINDOW_MARGIN - BUTTON_HEIGHT)
#define EXECUTABLE_PATH_WIDTH(width) (width - 4 * WINDOW_MARGIN - START_BUTTON_WIDTH - CHECKBOX_WIDTH)
#define START_BUTTON_WIDTH 64
#define CHECKBOX_WIDTH 96

void window_add_to_file(window_t *const window, const char *const line);

void window_add_to_lines(window_t *const window, const char *const line);

typedef void (*const window_add_t)(window_t *const window, const char *const line);

#endif // WINDOW_H
