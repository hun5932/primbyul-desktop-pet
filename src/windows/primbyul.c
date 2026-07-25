#define WIN32_LEAN_AND_MEAN
#define COBJMACROS

/*
 * Primbyul desktop pet for Windows.
 *
 * Architecture:
 *   - A borderless layered Win32 window renders premultiplied BGRA pixels.
 *   - Only four 512x512 PNG keyframes for the active action are decoded.
 *   - A 25 ms scheduler advances action frames and natural blink timing.
 *   - The notification-area menu owns all settings and remains available
 *     when the pet is hidden or mouse click-through is enabled.
 *
 * Start with docs/CODE-WALKTHROUGH.md before changing behavior or resources.
 */

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objidl.h>
#include <gdiplus/gdiplus.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define APP_NAME L"Primbyul"
#define APP_TITLE L"프림별 데스크톱 펫"
#define MUTEX_NAME L"PrimbyulDesktopPet_Native"

#define IDI_PRIMBYUL 102
#define WM_TRAY (WM_APP + 1)
#define WM_TASKBAR_CREATED (WM_APP + 2)
#define TIMER_ANIMATION 1
#define TIMER_TRAY_RETRY 2

#define CMD_SHOW 1001
#define CMD_PAUSE 1002
#define CMD_WAG 1003
#define CMD_PLAY 1004
#define CMD_WATCH 1005
#define CMD_CLICK_THROUGH 1006
#define CMD_TOPMOST 1007
#define CMD_SIZE_SMALL 1008
#define CMD_SIZE_NORMAL 1009
#define CMD_SIZE_LARGE 1010
#define CMD_RESET_POSITION 1011
#define CMD_AUTOSTART 1012
#define CMD_REMOVE_AND_EXIT 1013
#define CMD_EXIT 1014
#define CMD_SETTINGS 1015
#define CMD_MODE_AUTO 1016
#define CMD_MODE_SIT 1017
#define CMD_APPEARANCE_ADULT 1018
#define CMD_APPEARANCE_PUPPY 1019
#define CMD_RUN 1020
#define CMD_MODE_QUIET 1021
#define CMD_HIDE 1022

#define CTRL_MODE_AUTO 2001
#define CTRL_MODE_SIT 2002
#define CTRL_AUTOSTART 2003
#define CTRL_APPEARANCE_ADULT 2004
#define CTRL_APPEARANCE_PUPPY 2005
#define CTRL_MODE_QUIET 2006

#define SOURCE_WIDTH 512
#define SOURCE_HEIGHT 512
#define CELL_WIDTH 220
#define CELL_HEIGHT 220
#define FRAME_BYTES ((size_t)SOURCE_WIDTH * (size_t)SOURCE_HEIGHT * 4u)
#define TIMER_TICK_MS 25
#define RESOURCE_ADULT_BASE 1100
#define RESOURCE_PUPPY_BASE 2100

typedef struct AnimationState {
    const int *sequence;
    int sequence_count;
    UINT delay_ms;
    int loops;
} AnimationState;

enum {
    STATE_IDLE = 0,
    STATE_RUN_RIGHT,
    STATE_RUN_LEFT,
    STATE_WAG,
    STATE_PLAY,
    STATE_WATCH,
    STATE_SIT
};

enum {
    MODE_AUTO = 0,
    MODE_SIT = 1,
    MODE_QUIET = 2
};

enum {
    APPEARANCE_ADULT = 0,
    APPEARANCE_PUPPY = 1
};

static const int k_idle_sequence[] = {0, 0, 0, 0};
static const int k_run_sequence[] = {0, 1, 2, 3};
static const int k_wag_sequence[] = {0, 1, 2, 3};
static const int k_play_sequence[] = {0, 1, 2, 3, 2, 1, 0, 0};
static const int k_watch_sequence[] = {0, 1, 2, 1, 3, 1, 0, 1};
static const int k_sit_sequence[] = {0, 0, 0, 0};

static const AnimationState k_states[] = {
    {k_idle_sequence, ARRAYSIZE(k_idle_sequence), 140, 1},
    {k_run_sequence, ARRAYSIZE(k_run_sequence), 85, 5},
    {k_run_sequence, ARRAYSIZE(k_run_sequence), 85, 5},
    {k_wag_sequence, ARRAYSIZE(k_wag_sequence), 115, 4},
    {k_play_sequence, ARRAYSIZE(k_play_sequence), 120, 2},
    {k_watch_sequence, ARRAYSIZE(k_watch_sequence), 210, 2},
    {k_sit_sequence, ARRAYSIZE(k_sit_sequence), 140, 1}
};

static HINSTANCE g_instance;
static ULONG_PTR g_gdiplus_token;
static HWND g_window;
static HWND g_settings_window;
static HANDLE g_mutex;
static NOTIFYICONDATAW g_tray;
static HICON g_icon;
static BYTE *g_animation_frames[4];
static HDC g_frame_dc;
static HBITMAP g_frame_bitmap;
static HGDIOBJ g_old_bitmap;
static BYTE *g_frame_bits;
static int g_frame_width;
static int g_frame_height;
static int g_scale_percent = 100;
static int g_state = STATE_IDLE;
static int g_behavior_mode = MODE_AUTO;
static int g_appearance_mode = APPEARANCE_ADULT;
static int g_frame_index;
static int g_completed_loops;
static int g_blink_phase;
static ULONGLONG g_next_frame_at;
static ULONGLONG g_next_blink_at;
static ULONGLONG g_state_end_at;
static UINT g_taskbar_created_message;
static BOOL g_paused;
static BOOL g_click_through;
static BOOL g_topmost = TRUE;
static BOOL g_dragging;
static BOOL g_remove_settings_on_exit;
static BOOL g_tray_added;
static POINT g_drag_cursor;
static POINT g_drag_window;
static WCHAR g_settings_dir[MAX_PATH];
static WCHAR g_settings_path[MAX_PATH];

static void JoinPath(WCHAR *out, size_t out_count, const WCHAR *left, const WCHAR *right) {
    size_t left_len = lstrlenW(left);
    if (left_len + 1 >= out_count) {
        out[0] = L'\0';
        return;
    }
    lstrcpynW(out, left, (int)out_count);
    if (left_len > 0 && out[left_len - 1] != L'\\') {
        out[left_len++] = L'\\';
        out[left_len] = L'\0';
    }
    lstrcpynW(out + left_len, right, (int)(out_count - left_len));
}

static void InitializeSettingsPaths(void) {
    WCHAR app_data[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, app_data))) {
        GetTempPathW(MAX_PATH, app_data);
    }
    JoinPath(g_settings_dir, MAX_PATH, app_data, L"Primbyul");
    CreateDirectoryW(g_settings_dir, NULL);
    JoinPath(g_settings_path, MAX_PATH, g_settings_dir, L"settings.ini");
}

static void WriteIntSetting(const WCHAR *key, int value) {
    WCHAR buffer[32];
    wsprintfW(buffer, L"%d", value);
    WritePrivateProfileStringW(L"Primbyul", key, buffer, g_settings_path);
}

static int ReadIntSetting(const WCHAR *key, int fallback) {
    return GetPrivateProfileIntW(L"Primbyul", key, fallback, g_settings_path);
}

static BOOL DecodeFrameResource(int resource_id, BYTE **output_frame) {
    if (!output_frame) return FALSE;
    *output_frame = NULL;

    HRSRC resource = FindResourceW(
        g_instance,
        MAKEINTRESOURCEW(resource_id),
        RT_RCDATA
    );
    if (!resource) return FALSE;

    HGLOBAL loaded = LoadResource(g_instance, resource);
    DWORD resource_size = SizeofResource(g_instance, resource);
    const BYTE *png_bytes = loaded ? (const BYTE *)LockResource(loaded) : NULL;
    if (!png_bytes || resource_size == 0) return FALSE;

    HGLOBAL png_copy = GlobalAlloc(GMEM_MOVEABLE, resource_size);
    if (!png_copy) return FALSE;
    void *copy_bytes = GlobalLock(png_copy);
    if (!copy_bytes) {
        GlobalFree(png_copy);
        return FALSE;
    }
    memcpy(copy_bytes, png_bytes, resource_size);
    GlobalUnlock(png_copy);

    IStream *stream = NULL;
    if (FAILED(CreateStreamOnHGlobal(png_copy, TRUE, &stream))) {
        GlobalFree(png_copy);
        return FALSE;
    }

    GpBitmap *bitmap = NULL;
    BYTE *decoded_frame = NULL;
    UINT width = 0;
    UINT height = 0;
    BOOL success = FALSE;
    if (GdipCreateBitmapFromStream(stream, &bitmap) != Ok || !bitmap) goto cleanup;
    if (GdipGetImageWidth((GpImage *)bitmap, &width) != Ok ||
        GdipGetImageHeight((GpImage *)bitmap, &height) != Ok ||
        width != SOURCE_WIDTH ||
        height != SOURCE_HEIGHT) {
        goto cleanup;
    }

    GpRect rect = {0, 0, SOURCE_WIDTH, SOURCE_HEIGHT};
    BitmapData bitmap_data;
    ZeroMemory(&bitmap_data, sizeof(bitmap_data));
    if (GdipBitmapLockBits(
        bitmap,
        &rect,
        ImageLockModeRead,
        PixelFormat32bppARGB,
        &bitmap_data
    ) != Ok) {
        goto cleanup;
    }

    decoded_frame = (BYTE *)HeapAlloc(GetProcessHeap(), 0, FRAME_BYTES);
    if (decoded_frame) {
        for (int y = 0; y < SOURCE_HEIGHT; ++y) {
            const BYTE *source_row;
            if (bitmap_data.Stride >= 0) {
                source_row = (const BYTE *)bitmap_data.Scan0 + (size_t)y * (size_t)bitmap_data.Stride;
            } else {
                source_row = (const BYTE *)bitmap_data.Scan0 +
                    (size_t)(SOURCE_HEIGHT - 1 - y) * (size_t)(-bitmap_data.Stride);
            }
            BYTE *destination_row =
                decoded_frame + (size_t)y * (size_t)SOURCE_WIDTH * 4u;
            for (int x = 0; x < SOURCE_WIDTH; ++x) {
                const BYTE b = source_row[x * 4 + 0];
                const BYTE g = source_row[x * 4 + 1];
                const BYTE r = source_row[x * 4 + 2];
                const BYTE a = source_row[x * 4 + 3];
                destination_row[x * 4 + 0] = (BYTE)(((unsigned int)b * a + 127u) / 255u);
                destination_row[x * 4 + 1] = (BYTE)(((unsigned int)g * a + 127u) / 255u);
                destination_row[x * 4 + 2] = (BYTE)(((unsigned int)r * a + 127u) / 255u);
                destination_row[x * 4 + 3] = a;
            }
        }
        success = TRUE;
    }
    GdipBitmapUnlockBits(bitmap, &bitmap_data);

cleanup:
    if (bitmap) GdipDisposeImage((GpImage *)bitmap);
    IStream_Release(stream);
    if (!success && decoded_frame) {
        HeapFree(GetProcessHeap(), 0, decoded_frame);
    } else if (success) {
        *output_frame = decoded_frame;
    }
    return success;
}

static void FreeAnimationFrames(BYTE **frames) {
    if (!frames) return;
    for (int index = 0; index < 4; ++index) {
        if (frames[index]) {
            HeapFree(GetProcessHeap(), 0, frames[index]);
            frames[index] = NULL;
        }
    }
}

static int ResourceIdForFrame(int appearance, int state, int frame_key) {
    int action = state;
    if (action == STATE_RUN_LEFT) action = STATE_RUN_RIGHT;
    if (action > STATE_SIT) action = STATE_IDLE;
    return (appearance == APPEARANCE_PUPPY ? RESOURCE_PUPPY_BASE : RESOURCE_ADULT_BASE)
        + action * 10 + frame_key;
}

static BOOL LoadAnimationFrames(int appearance, int state) {
    BYTE *loaded[4] = {NULL, NULL, NULL, NULL};
    for (int key = 0; key < 4; ++key) {
        if (!DecodeFrameResource(
            ResourceIdForFrame(appearance, state, key),
            &loaded[key]
        )) {
            FreeAnimationFrames(loaded);
            return FALSE;
        }
    }
    FreeAnimationFrames(g_animation_frames);
    memcpy(g_animation_frames, loaded, sizeof(loaded));
    return TRUE;
}

static void DestroyFrameSurface(void) {
    if (g_frame_dc && g_old_bitmap) SelectObject(g_frame_dc, g_old_bitmap);
    if (g_frame_bitmap) DeleteObject(g_frame_bitmap);
    if (g_frame_dc) DeleteDC(g_frame_dc);
    g_frame_dc = NULL;
    g_frame_bitmap = NULL;
    g_old_bitmap = NULL;
    g_frame_bits = NULL;
}

static BOOL CreateFrameSurface(int width, int height) {
    DestroyFrameSurface();

    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(NULL);
    g_frame_dc = CreateCompatibleDC(screen);
    g_frame_bitmap = CreateDIBSection(
        screen,
        &info,
        DIB_RGB_COLORS,
        (void **)&g_frame_bits,
        NULL,
        0
    );
    ReleaseDC(NULL, screen);

    if (!g_frame_dc || !g_frame_bitmap || !g_frame_bits) {
        DestroyFrameSurface();
        return FALSE;
    }

    g_old_bitmap = SelectObject(g_frame_dc, g_frame_bitmap);
    g_frame_width = width;
    g_frame_height = height;
    return TRUE;
}

static void RenderFrame(void) {
    if (!g_animation_frames[0] || !g_frame_bits || !g_window) return;

    const AnimationState *state = &k_states[g_state];
    int key = state->sequence[g_frame_index % state->sequence_count];
    if ((g_state == STATE_IDLE || g_state == STATE_SIT) && g_blink_phase > 0) {
        key = g_blink_phase == 2 ? 2 : 1;
    }
    if (key < 0 || key > 3 || !g_animation_frames[key]) key = 0;
    const BYTE *frame = g_animation_frames[key];
    const BOOL mirror = g_state == STATE_RUN_LEFT;

    for (int y = 0; y < g_frame_height; ++y) {
        int64_t source_y_fixed =
            ((((int64_t)(2 * y + 1) * SOURCE_HEIGHT) << 15) / g_frame_height) - 32768;
        int source_y = (int)(source_y_fixed >> 16);
        unsigned int weight_y = (unsigned int)(source_y_fixed & 0xffff);
        if (source_y < 0) {
            source_y = 0;
            weight_y = 0;
        }
        int source_y_next = source_y + 1;
        if (source_y_next >= SOURCE_HEIGHT) {
            source_y_next = SOURCE_HEIGHT - 1;
            source_y = source_y_next;
            weight_y = 0;
        }
        BYTE *destination_row = g_frame_bits + ((size_t)y * (size_t)g_frame_width * 4u);

        for (int x = 0; x < g_frame_width; ++x) {
            int64_t source_x_fixed =
                ((((int64_t)(2 * x + 1) * SOURCE_WIDTH) << 15) / g_frame_width) - 32768;
            int source_x = (int)(source_x_fixed >> 16);
            unsigned int weight_x = (unsigned int)(source_x_fixed & 0xffff);
            if (source_x < 0) {
                source_x = 0;
                weight_x = 0;
            }
            int source_x_next = source_x + 1;
            if (source_x_next >= SOURCE_WIDTH) {
                source_x_next = SOURCE_WIDTH - 1;
                source_x = source_x_next;
                weight_x = 0;
            }
            if (mirror) {
                int mirrored_source = SOURCE_WIDTH - 1 - source_x;
                int mirrored_next = SOURCE_WIDTH - 1 - source_x_next;
                source_x = mirrored_source;
                source_x_next = mirrored_next;
            }

            const BYTE *top_left = frame + (
                ((size_t)source_y * (size_t)SOURCE_WIDTH +
                 (size_t)source_x) * 4u
            );
            const BYTE *top_right = frame + (
                ((size_t)source_y * (size_t)SOURCE_WIDTH +
                 (size_t)source_x_next) * 4u
            );
            const BYTE *bottom_left = frame + (
                ((size_t)source_y_next * (size_t)SOURCE_WIDTH +
                 (size_t)source_x) * 4u
            );
            const BYTE *bottom_right = frame + (
                ((size_t)source_y_next * (size_t)SOURCE_WIDTH +
                 (size_t)source_x_next) * 4u
            );
            BYTE *destination = destination_row + (size_t)x * 4u;
            for (int channel = 0; channel < 4; ++channel) {
                unsigned int top =
                    ((unsigned int)top_left[channel] * (65536u - weight_x) +
                     (unsigned int)top_right[channel] * weight_x + 32768u) >> 16;
                unsigned int bottom =
                    ((unsigned int)bottom_left[channel] * (65536u - weight_x) +
                     (unsigned int)bottom_right[channel] * weight_x + 32768u) >> 16;
                destination[channel] = (BYTE)(
                    (top * (65536u - weight_y) + bottom * weight_y + 32768u) >> 16
                );
            }
        }
    }

    RECT rect;
    GetWindowRect(g_window, &rect);
    POINT destination = {rect.left, rect.top};
    SIZE size = {g_frame_width, g_frame_height};
    POINT source = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(
        g_window,
        NULL,
        &destination,
        &size,
        g_frame_dc,
        &source,
        0,
        &blend,
        ULW_ALPHA
    );
}

static BOOL ApplyAppearanceMode(int mode) {
    int normalized_mode =
        mode == APPEARANCE_PUPPY ? APPEARANCE_PUPPY : APPEARANCE_ADULT;
    if (normalized_mode == g_appearance_mode && g_animation_frames[0]) {
        RenderFrame();
        return TRUE;
    }

    if (!LoadAnimationFrames(normalized_mode, g_state)) {
        return FALSE;
    }
    g_appearance_mode = normalized_mode;
    RenderFrame();
    return TRUE;
}

static void SetAnimationState(int state) {
    if (state < STATE_IDLE || state > STATE_SIT) state = STATE_IDLE;
    int old_state = g_state;
    if (state != old_state && !LoadAnimationFrames(g_appearance_mode, state)) {
        MessageBoxW(
            g_window,
            L"동작 프레임을 불러오지 못했습니다.",
            APP_TITLE,
            MB_OK | MB_ICONERROR
        );
        return;
    }
    g_state = state;
    g_frame_index = 0;
    g_completed_loops = 0;
    g_blink_phase = 0;
    ULONGLONG now = GetTickCount64();
    g_next_frame_at = now + k_states[g_state].delay_ms;
    g_next_blink_at = now + 3000u + (UINT)(rand() % 5001);
    if (state == STATE_IDLE) {
        UINT dwell = g_behavior_mode == MODE_QUIET
            ? 9000u + (UINT)(rand() % 7001)
            : 5000u + (UINT)(rand() % 6001);
        g_state_end_at = now + dwell;
    } else if (state == STATE_SIT) {
        g_state_end_at = 0;
    }
    RenderFrame();
}

static void SetCalmSit(void) {
    SetAnimationState(STATE_SIT);
}

static void ApplyBehaviorMode(int mode) {
    g_behavior_mode =
        mode == MODE_SIT ? MODE_SIT :
        mode == MODE_QUIET ? MODE_QUIET :
        MODE_AUTO;
    if (g_behavior_mode == MODE_SIT) {
        SetCalmSit();
    } else {
        SetAnimationState(STATE_IDLE);
    }
}

static int ChooseRunState(void) {
    RECT rect;
    MONITORINFO monitor_info;
    ZeroMemory(&monitor_info, sizeof(monitor_info));
    monitor_info.cbSize = sizeof(monitor_info);

    if (GetWindowRect(g_window, &rect) &&
        GetMonitorInfoW(
            MonitorFromWindow(g_window, MONITOR_DEFAULTTONEAREST),
            &monitor_info
        )) {
        int window_center = rect.left + (rect.right - rect.left) / 2;
        int work_center =
            monitor_info.rcWork.left +
            (monitor_info.rcWork.right - monitor_info.rcWork.left) / 2;
        if (window_center < work_center) return STATE_RUN_RIGHT;
        if (window_center > work_center) return STATE_RUN_LEFT;
    }
    return (rand() & 1) ? STATE_RUN_RIGHT : STATE_RUN_LEFT;
}

static int SelectNextState(void) {
    int roll = rand() % 100;
    if (g_behavior_mode == MODE_QUIET) {
        if (roll < 70) return STATE_IDLE;
        if (roll < 86) return STATE_WATCH;
        if (roll < 96) return STATE_WAG;
        return STATE_PLAY;
    }
    if (roll < 55) return STATE_IDLE;
    if (roll < 67) return ChooseRunState();
    if (roll < 82) return STATE_WAG;
    if (roll < 91) return STATE_PLAY;
    return STATE_WATCH;
}

static void MoveRunStep(void) {
    if (g_dragging ||
        (g_state != STATE_RUN_RIGHT && g_state != STATE_RUN_LEFT)) {
        return;
    }

    RECT rect;
    if (!GetWindowRect(g_window, &rect)) return;

    MONITORINFO monitor_info;
    ZeroMemory(&monitor_info, sizeof(monitor_info));
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(
        MonitorFromWindow(g_window, MONITOR_DEFAULTTONEAREST),
        &monitor_info
    )) {
        return;
    }

    int step = MulDiv(7, g_scale_percent, 100);
    if (step < 4) step = 4;
    if (g_state == STATE_RUN_LEFT) step = -step;

    int next_x = rect.left + step;
    int width = rect.right - rect.left;
    if (next_x <= monitor_info.rcWork.left) {
        next_x = monitor_info.rcWork.left;
        g_state = STATE_RUN_RIGHT;
    } else if (next_x + width >= monitor_info.rcWork.right) {
        next_x = monitor_info.rcWork.right - width;
        g_state = STATE_RUN_LEFT;
    }

    SetWindowPos(
        g_window,
        NULL,
        next_x,
        rect.top,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    );
}

static void SetClickThrough(BOOL enabled) {
    g_click_through = enabled;
    LONG_PTR style = GetWindowLongPtrW(g_window, GWL_EXSTYLE);
    if (enabled) style |= WS_EX_TRANSPARENT;
    else style &= ~((LONG_PTR)WS_EX_TRANSPARENT);
    SetWindowLongPtrW(g_window, GWL_EXSTYLE, style);
    SetWindowPos(
        g_window,
        NULL,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
    );
}

static void ResetPosition(void) {
    RECT work_area;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    SetWindowPos(
        g_window,
        NULL,
        work_area.right - g_frame_width - 24,
        work_area.bottom - g_frame_height - 24,
        g_frame_width,
        g_frame_height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
    );
    RenderFrame();
}

static void SetPetScale(int percent) {
    if (percent < 60) percent = 60;
    if (percent > 180) percent = 180;
    g_scale_percent = percent;

    int new_width = MulDiv(CELL_WIDTH, percent, 100);
    int new_height = MulDiv(CELL_HEIGHT, percent, 100);
    RECT rect;
    GetWindowRect(g_window, &rect);

    if (!CreateFrameSurface(new_width, new_height)) {
        MessageBoxW(g_window, L"프림별 화면을 만들 수 없습니다.", APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    SetWindowPos(
        g_window,
        g_topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
        rect.left,
        rect.top,
        new_width,
        new_height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );
    RenderFrame();
}

static BOOL IsAutostartEnabled(void) {
    HKEY key;
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_QUERY_VALUE,
        &key
    ) != ERROR_SUCCESS) {
        return FALSE;
    }

    WCHAR value[MAX_PATH * 2];
    DWORD type = 0;
    DWORD size = sizeof(value);
    LSTATUS status = RegQueryValueExW(key, APP_NAME, NULL, &type, (BYTE *)value, &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_SZ;
}

static BOOL SetAutostart(BOOL enabled) {
    HKEY key;
    if (RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        NULL,
        0,
        KEY_SET_VALUE,
        NULL,
        &key,
        NULL
    ) != ERROR_SUCCESS) {
        return FALSE;
    }

    LSTATUS status;
    if (enabled) {
        WCHAR executable[MAX_PATH];
        WCHAR quoted[MAX_PATH * 2];
        GetModuleFileNameW(NULL, executable, MAX_PATH);
        wsprintfW(quoted, L"\"%s\"", executable);
        status = RegSetValueExW(
            key,
            APP_NAME,
            0,
            REG_SZ,
            (const BYTE *)quoted,
            (DWORD)((lstrlenW(quoted) + 1) * sizeof(WCHAR))
        );
    } else {
        status = RegDeleteValueW(key, APP_NAME);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

static void RefreshAutostartPathIfEnabled(void) {
    if (IsAutostartEnabled()) {
        SetAutostart(TRUE);
    }
}

static void SaveSettings(void) {
    RECT rect;
    GetWindowRect(g_window, &rect);
    WriteIntSetting(L"X", rect.left);
    WriteIntSetting(L"Y", rect.top);
    WriteIntSetting(L"Scale", g_scale_percent);
    WriteIntSetting(L"ClickThrough", g_click_through ? 1 : 0);
    WriteIntSetting(L"Topmost", g_topmost ? 1 : 0);
    WriteIntSetting(L"BehaviorMode", g_behavior_mode);
    WriteIntSetting(L"AppearanceMode", g_appearance_mode);
}

static void SetControlFont(HWND control) {
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

static HWND AddSettingsControl(
    HWND parent,
    const WCHAR *class_name,
    const WCHAR *text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    int identifier
) {
    HWND control = CreateWindowExW(
        0,
        class_name,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        parent,
        (HMENU)(INT_PTR)identifier,
        g_instance,
        NULL
    );
    if (control) SetControlFont(control);
    return control;
}

static LRESULT CALLBACK SettingsWindowProcedure(
    HWND window,
    UINT message,
    WPARAM w_param,
    LPARAM l_param
) {
    (void)l_param;
    switch (message) {
        case WM_CREATE: {
            AddSettingsControl(window, L"STATIC", L"프림 모습", 0, 20, 18, 320, 20, 0);
            HWND adult_button = AddSettingsControl(
                window, L"BUTTON", L"성견 프림",
                BS_AUTORADIOBUTTON | WS_GROUP,
                28, 46, 310, 24, CTRL_APPEARANCE_ADULT
            );
            HWND puppy_button = AddSettingsControl(
                window, L"BUTTON", L"어릴 때 프림",
                BS_AUTORADIOBUTTON,
                28, 76, 310, 24, CTRL_APPEARANCE_PUPPY
            );

            AddSettingsControl(window, L"STATIC", L"동작 모드", 0, 20, 118, 320, 20, 0);
            HWND auto_button = AddSettingsControl(
                window, L"BUTTON", L"자동으로 놀기",
                BS_AUTORADIOBUTTON | WS_GROUP,
                28, 146, 310, 24, CTRL_MODE_AUTO
            );
            HWND sit_button = AddSettingsControl(
                window, L"BUTTON", L"얌전히 앉아 있기",
                BS_AUTORADIOBUTTON,
                28, 176, 310, 24, CTRL_MODE_SIT
            );
            HWND quiet_button = AddSettingsControl(
                window, L"BUTTON", L"조용히 지켜보기 (달리기 없음)",
                BS_AUTORADIOBUTTON,
                28, 206, 310, 24, CTRL_MODE_QUIET
            );
            AddSettingsControl(
                window, L"STATIC",
                L"앉아 있어도 우클릭하면 원하는 동작을 한 번 시킬 수 있어요.",
                0, 46, 236, 294, 36, 0
            );
            AddSettingsControl(window, L"STATIC", L"자동 실행", 0, 20, 280, 320, 20, 0);
            HWND autostart_button = AddSettingsControl(
                window, L"BUTTON", L"Windows 로그인 시 프림별 자동 실행",
                BS_AUTOCHECKBOX, 28, 307, 320, 24, CTRL_AUTOSTART
            );
            AddSettingsControl(
                window, L"BUTTON", L"저장",
                BS_DEFPUSHBUTTON, 196, 350, 72, 28, IDOK
            );
            AddSettingsControl(
                window, L"BUTTON", L"취소",
                BS_PUSHBUTTON, 276, 350, 72, 28, IDCANCEL
            );

            SendMessageW(
                g_appearance_mode == APPEARANCE_PUPPY
                    ? puppy_button
                    : adult_button,
                BM_SETCHECK,
                BST_CHECKED,
                0
            );

            HWND behavior_button =
                g_behavior_mode == MODE_SIT ? sit_button :
                g_behavior_mode == MODE_QUIET ? quiet_button :
                auto_button;
            SendMessageW(behavior_button, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(
                autostart_button,
                BM_SETCHECK,
                IsAutostartEnabled() ? BST_CHECKED : BST_UNCHECKED,
                0
            );
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(w_param) == IDOK) {
                int selected_mode = MODE_AUTO;
                if (SendDlgItemMessageW(
                    window, CTRL_MODE_SIT, BM_GETCHECK, 0, 0
                ) == BST_CHECKED) {
                    selected_mode = MODE_SIT;
                } else if (SendDlgItemMessageW(
                    window, CTRL_MODE_QUIET, BM_GETCHECK, 0, 0
                ) == BST_CHECKED) {
                    selected_mode = MODE_QUIET;
                }
                int selected_appearance =
                    SendDlgItemMessageW(
                        window,
                        CTRL_APPEARANCE_PUPPY,
                        BM_GETCHECK,
                        0,
                        0
                    ) == BST_CHECKED
                    ? APPEARANCE_PUPPY
                    : APPEARANCE_ADULT;
                BOOL selected_autostart =
                    SendDlgItemMessageW(window, CTRL_AUTOSTART, BM_GETCHECK, 0, 0)
                    == BST_CHECKED;

                if (!ApplyAppearanceMode(selected_appearance)) {
                    MessageBoxW(
                        window,
                        L"선택한 프림 이미지를 불러오지 못했습니다.",
                        APP_TITLE,
                        MB_OK | MB_ICONERROR
                    );
                    return 0;
                }
                if (!SetAutostart(selected_autostart)) {
                    MessageBoxW(
                        window,
                        L"자동 실행 설정을 변경하지 못했습니다.",
                        APP_TITLE,
                        MB_OK | MB_ICONERROR
                    );
                    return 0;
                }
                ApplyBehaviorMode(selected_mode);
                SaveSettings();
                DestroyWindow(window);
                return 0;
            }
            if (LOWORD(w_param) == IDCANCEL) {
                DestroyWindow(window);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            g_settings_window = NULL;
            return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

static void ShowSettingsWindow(void) {
    if (g_settings_window) {
        ShowWindow(g_settings_window, SW_SHOW);
        SetForegroundWindow(g_settings_window);
        return;
    }

    const int width = 380;
    const int height = 425;
    RECT owner_rect;
    GetWindowRect(g_window, &owner_rect);
    int x = owner_rect.left + (owner_rect.right - owner_rect.left - width) / 2;
    int y = owner_rect.top + (owner_rect.bottom - owner_rect.top - height) / 2;

    RECT work_area;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    if (x < work_area.left) x = work_area.left + 16;
    if (y < work_area.top) y = work_area.top + 16;
    if (x + width > work_area.right) x = work_area.right - width - 16;
    if (y + height > work_area.bottom) y = work_area.bottom - height - 16;

    g_settings_window = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"PrimbyulSettingsWindow",
        L"프림별 설정",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x,
        y,
        width,
        height,
        g_window,
        NULL,
        g_instance,
        NULL
    );
    if (g_settings_window) {
        SendMessageW(g_settings_window, WM_SETICON, ICON_BIG, (LPARAM)g_icon);
        SendMessageW(g_settings_window, WM_SETICON, ICON_SMALL, (LPARAM)g_icon);
        ShowWindow(g_settings_window, SW_SHOW);
        SetForegroundWindow(g_settings_window);
    }
}

static void ShowContextMenu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, CMD_SETTINGS, L"설정...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(
        menu,
        MF_STRING,
        IsWindowVisible(g_window) ? CMD_HIDE : CMD_SHOW,
        IsWindowVisible(g_window) ? L"프림별 잠시 숨기기" : L"프림별 보이기"
    );
    AppendMenuW(menu, MF_STRING | (g_paused ? MF_CHECKED : 0), CMD_PAUSE,
                g_paused ? L"움직임 다시 시작" : L"움직임 일시정지");
    AppendMenuW(menu, MF_STRING, CMD_RUN, L"뛰어다니기");
    AppendMenuW(menu, MF_STRING, CMD_WAG, L"꼬리 흔들기");
    AppendMenuW(menu, MF_STRING, CMD_PLAY, L"장난치기");
    AppendMenuW(menu, MF_STRING, CMD_WATCH, L"옆에서 지켜보기");

    HMENU appearance_menu = CreatePopupMenu();
    AppendMenuW(
        appearance_menu,
        MF_STRING | (g_appearance_mode == APPEARANCE_ADULT ? MF_CHECKED : 0),
        CMD_APPEARANCE_ADULT,
        L"성견 프림"
    );
    AppendMenuW(
        appearance_menu,
        MF_STRING | (g_appearance_mode == APPEARANCE_PUPPY ? MF_CHECKED : 0),
        CMD_APPEARANCE_PUPPY,
        L"어릴 때 프림"
    );
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)appearance_menu, L"프림 모습");

    HMENU behavior_menu = CreatePopupMenu();
    AppendMenuW(
        behavior_menu,
        MF_STRING | (g_behavior_mode == MODE_AUTO ? MF_CHECKED : 0),
        CMD_MODE_AUTO,
        L"자동으로 놀기"
    );
    AppendMenuW(
        behavior_menu,
        MF_STRING | (g_behavior_mode == MODE_SIT ? MF_CHECKED : 0),
        CMD_MODE_SIT,
        L"얌전히 앉아 있기"
    );
    AppendMenuW(
        behavior_menu,
        MF_STRING | (g_behavior_mode == MODE_QUIET ? MF_CHECKED : 0),
        CMD_MODE_QUIET,
        L"조용히 지켜보기"
    );
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)behavior_menu, L"동작 모드");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (g_click_through ? MF_CHECKED : 0),
                CMD_CLICK_THROUGH, L"마우스 클릭 통과");
    AppendMenuW(menu, MF_STRING | (g_topmost ? MF_CHECKED : 0),
                CMD_TOPMOST, L"항상 위에 표시");

    HMENU size_menu = CreatePopupMenu();
    AppendMenuW(size_menu, MF_STRING | (g_scale_percent == 75 ? MF_CHECKED : 0),
                CMD_SIZE_SMALL, L"작게 (75%)");
    AppendMenuW(size_menu, MF_STRING | (g_scale_percent == 100 ? MF_CHECKED : 0),
                CMD_SIZE_NORMAL, L"보통 (100%)");
    AppendMenuW(size_menu, MF_STRING | (g_scale_percent == 135 ? MF_CHECKED : 0),
                CMD_SIZE_LARGE, L"크게 (135%)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)size_menu, L"크기");

    AppendMenuW(menu, MF_STRING, CMD_RESET_POSITION, L"오른쪽 아래로 이동");
    AppendMenuW(menu, MF_STRING | (IsAutostartEnabled() ? MF_CHECKED : 0),
                CMD_AUTOSTART, L"Windows 시작 시 자동 실행");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, CMD_REMOVE_AND_EXIT, L"설정 제거 후 종료");
    AppendMenuW(menu, MF_STRING, CMD_EXIT, L"종료");

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, owner, NULL);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static void AddTrayIcon(void) {
    ZeroMemory(&g_tray, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WM_TRAY;
    g_tray.hIcon = g_icon;
    lstrcpynW(g_tray.szTip, APP_TITLE, ARRAYSIZE(g_tray.szTip));
    g_tray_added = Shell_NotifyIconW(NIM_ADD, &g_tray);
    g_tray.uVersion = NOTIFYICON_VERSION_4;
    if (g_tray_added) {
        Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
        KillTimer(g_window, TIMER_TRAY_RETRY);
    } else {
        SetTimer(g_window, TIMER_TRAY_RETRY, 2000, NULL);
    }
}

static void RemoveTrayIcon(void) {
    if (g_tray.cbSize && g_tray_added) Shell_NotifyIconW(NIM_DELETE, &g_tray);
    g_tray_added = FALSE;
    ZeroMemory(&g_tray, sizeof(g_tray));
}

static void HandleCommand(HWND window, UINT command) {
    switch (command) {
        case CMD_SETTINGS:
            ShowSettingsWindow();
            break;
        case CMD_SHOW:
            ShowWindow(window, SW_SHOWNOACTIVATE);
            SetWindowPos(
                window,
                g_topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
            );
            RenderFrame();
            break;
        case CMD_HIDE:
            ShowWindow(window, SW_HIDE);
            break;
        case CMD_PAUSE:
            g_paused = !g_paused;
            if (!g_paused) {
                ULONGLONG now = GetTickCount64();
                g_next_frame_at = now + k_states[g_state].delay_ms;
                if (g_blink_phase == 0) {
                    g_next_blink_at = now + 3000u + (UINT)(rand() % 5001);
                }
            }
            break;
        case CMD_RUN:
            SetAnimationState(ChooseRunState());
            break;
        case CMD_WAG:
            SetAnimationState(STATE_WAG);
            break;
        case CMD_PLAY:
            SetAnimationState(STATE_PLAY);
            break;
        case CMD_WATCH:
            SetAnimationState(STATE_WATCH);
            break;
        case CMD_MODE_AUTO:
            ApplyBehaviorMode(MODE_AUTO);
            SaveSettings();
            break;
        case CMD_MODE_SIT:
            ApplyBehaviorMode(MODE_SIT);
            SaveSettings();
            break;
        case CMD_MODE_QUIET:
            ApplyBehaviorMode(MODE_QUIET);
            SaveSettings();
            break;
        case CMD_APPEARANCE_ADULT:
            if (!ApplyAppearanceMode(APPEARANCE_ADULT)) {
                MessageBoxW(
                    window,
                    L"성견 프림 이미지를 불러오지 못했습니다.",
                    APP_TITLE,
                    MB_OK | MB_ICONERROR
                );
            } else {
                SaveSettings();
            }
            break;
        case CMD_APPEARANCE_PUPPY:
            if (!ApplyAppearanceMode(APPEARANCE_PUPPY)) {
                MessageBoxW(
                    window,
                    L"어릴 때 프림 이미지를 불러오지 못했습니다.",
                    APP_TITLE,
                    MB_OK | MB_ICONERROR
                );
            } else {
                SaveSettings();
            }
            break;
        case CMD_CLICK_THROUGH:
            SetClickThrough(!g_click_through);
            break;
        case CMD_TOPMOST:
            g_topmost = !g_topmost;
            SetWindowPos(
                window,
                g_topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
            );
            break;
        case CMD_SIZE_SMALL:
            SetPetScale(75);
            break;
        case CMD_SIZE_NORMAL:
            SetPetScale(100);
            break;
        case CMD_SIZE_LARGE:
            SetPetScale(135);
            break;
        case CMD_RESET_POSITION:
            ResetPosition();
            break;
        case CMD_AUTOSTART: {
            BOOL enable = !IsAutostartEnabled();
            if (!SetAutostart(enable)) {
                MessageBoxW(
                    window,
                    L"자동 실행 설정을 변경하지 못했습니다.",
                    APP_TITLE,
                    MB_OK | MB_ICONERROR
                );
            }
            break;
        }
        case CMD_REMOVE_AND_EXIT:
            SetAutostart(FALSE);
            g_remove_settings_on_exit = TRUE;
            MessageBoxW(
                window,
                L"자동 실행과 저장된 설정을 제거했습니다.\n"
                L"종료 후 현재 Primbyul EXE 파일을 삭제하면 완전히 제거됩니다.",
                APP_TITLE,
                MB_OK | MB_ICONINFORMATION
            );
            DestroyWindow(window);
            break;
        case CMD_EXIT:
            DestroyWindow(window);
            break;
    }
}

static void ClampPetToWorkArea(void) {
    RECT rect;
    if (!g_window || !GetWindowRect(g_window, &rect)) return;
    MONITORINFO info;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(monitor, &info)) return;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x = rect.left;
    int y = rect.top;
    if (x < info.rcWork.left) x = info.rcWork.left;
    if (y < info.rcWork.top) y = info.rcWork.top;
    if (x + width > info.rcWork.right) x = info.rcWork.right - width;
    if (y + height > info.rcWork.bottom) y = info.rcWork.bottom - height;
    SetWindowPos(
        g_window, NULL, x, y, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    );
}

static void TickBlink(ULONGLONG now) {
    if (now < g_next_blink_at) return;
    switch (g_blink_phase) {
        case 0:
            g_blink_phase = 1;
            g_next_blink_at = now + 70;
            break;
        case 1:
            g_blink_phase = 2;
            g_next_blink_at = now + 90;
            break;
        case 2:
            g_blink_phase = 3;
            g_next_blink_at = now + 70;
            break;
        default:
            g_blink_phase = 0;
            g_next_blink_at = now + 3000u + (UINT)(rand() % 5001);
            break;
    }
    RenderFrame();
}

static void TickAnimation(void) {
    if (g_paused) return;
    ULONGLONG now = GetTickCount64();
    if (g_state == STATE_IDLE || g_state == STATE_SIT) {
        TickBlink(now);
        if (g_state == STATE_IDLE &&
            g_state_end_at != 0 &&
            now >= g_state_end_at &&
            g_blink_phase == 0) {
            SetAnimationState(SelectNextState());
        }
        return;
    }
    if (now < g_next_frame_at) return;

    const AnimationState *state = &k_states[g_state];
    g_next_frame_at = now + state->delay_ms;
    ++g_frame_index;
    if (g_frame_index >= state->sequence_count) {
        g_frame_index = 0;
        ++g_completed_loops;
        if (g_completed_loops >= state->loops) {
            if (g_behavior_mode == MODE_SIT) SetCalmSit();
            else SetAnimationState(SelectNextState());
            return;
        }
    }
    MoveRunStep();
    RenderFrame();
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    if (g_taskbar_created_message != 0 && message == g_taskbar_created_message) {
        g_tray_added = FALSE;
        AddTrayIcon();
        return 0;
    }
    switch (message) {
        case WM_CREATE:
            g_window = window;
            AddTrayIcon();
            return 0;

        case WM_TIMER:
            if (w_param == TIMER_ANIMATION) TickAnimation();
            else if (w_param == TIMER_TRAY_RETRY) AddTrayIcon();
            return 0;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_NCHITTEST: {
            if (g_click_through) return HTTRANSPARENT;
            POINT point = {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            ScreenToClient(window, &point);
            if (point.x < 0 || point.y < 0 ||
                point.x >= g_frame_width || point.y >= g_frame_height) {
                return HTTRANSPARENT;
            }
            const BYTE alpha = g_frame_bits[
                ((size_t)point.y * (size_t)g_frame_width + (size_t)point.x) * 4u + 3u
            ];
            return alpha < 18 ? HTTRANSPARENT : HTCLIENT;
        }

        case WM_LBUTTONDOWN: {
            if (!g_click_through) {
                RECT rect;
                GetCursorPos(&g_drag_cursor);
                GetWindowRect(window, &rect);
                g_drag_window.x = rect.left;
                g_drag_window.y = rect.top;
                g_dragging = TRUE;
                SetCapture(window);
            }
            return 0;
        }

        case WM_MOUSEMOVE:
            if (g_dragging && (w_param & MK_LBUTTON)) {
                POINT cursor;
                GetCursorPos(&cursor);
                SetWindowPos(
                    window,
                    NULL,
                    g_drag_window.x + cursor.x - g_drag_cursor.x,
                    g_drag_window.y + cursor.y - g_drag_cursor.y,
                    0,
                    0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                );
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_dragging) {
                g_dragging = FALSE;
                ReleaseCapture();
                ClampPetToWorkArea();
                SaveSettings();
            }
            return 0;

        case WM_LBUTTONDBLCLK:
            if (!g_click_through) SetAnimationState(STATE_WAG);
            return 0;

        case WM_RBUTTONUP:
            ShowContextMenu(window);
            return 0;

        case WM_TRAY:
            if (LOWORD(l_param) == WM_RBUTTONUP || LOWORD(l_param) == WM_CONTEXTMENU) {
                ShowContextMenu(window);
            } else if (LOWORD(l_param) == WM_LBUTTONDBLCLK) {
                HandleCommand(window, CMD_SHOW);
            }
            return 0;

        case WM_COMMAND:
            HandleCommand(window, LOWORD(w_param));
            return 0;

        case WM_DISPLAYCHANGE:
            ClampPetToWorkArea();
            RenderFrame();
            return 0;

        case WM_DPICHANGED: {
            RECT *suggested = (RECT *)l_param;
            if (suggested) {
                SetWindowPos(
                    window, NULL,
                    suggested->left, suggested->top,
                    0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                );
            }
            ClampPetToWorkArea();
            RenderFrame();
            return 0;
        }

        case WM_ENDSESSION:
            if (w_param) SaveSettings();
            return 0;

        case WM_DESTROY:
            if (g_remove_settings_on_exit) {
                DeleteFileW(g_settings_path);
                RemoveDirectoryW(g_settings_dir);
            } else {
                SaveSettings();
            }
            KillTimer(window, TIMER_ANIMATION);
            KillTimer(window, TIMER_TRAY_RETRY);
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command) {
    (void)previous;
    (void)command_line;
    (void)show_command;

    g_instance = instance;
    g_mutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (!g_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"프림별이 이미 실행 중입니다.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        if (g_mutex) CloseHandle(g_mutex);
        return 0;
    }

    GdiplusStartupInput startup_input;
    ZeroMemory(&startup_input, sizeof(startup_input));
    startup_input.GdiplusVersion = 1;
    if (GdiplusStartup(&g_gdiplus_token, &startup_input, NULL) != Ok) {
        MessageBoxW(
            NULL,
            L"프림별 그래픽 엔진을 시작하지 못했습니다.",
            APP_TITLE,
            MB_OK | MB_ICONERROR
        );
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
        return 1;
    }

    InitializeSettingsPaths();
    RefreshAutostartPathIfEnabled();
    g_taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    g_appearance_mode = ReadIntSetting(L"AppearanceMode", APPEARANCE_ADULT);
    if (g_appearance_mode != APPEARANCE_PUPPY) {
        g_appearance_mode = APPEARANCE_ADULT;
    }
    if (!LoadAnimationFrames(g_appearance_mode, STATE_IDLE)) {
        MessageBoxW(
            NULL,
            L"내장된 프림별 이미지를 불러오지 못했습니다.",
            APP_TITLE,
            MB_OK | MB_ICONERROR
        );
        GdiplusShutdown(g_gdiplus_token);
        CloseHandle(g_mutex);
        return 1;
    }

    srand((unsigned int)time(NULL));
    SetProcessDPIAware();

    WNDCLASSEXW window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_DBLCLKS;
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_HAND);
    window_class.lpszClassName = L"PrimbyulNativeWindow";
    g_icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_PRIMBYUL));
    if (!g_icon) g_icon = LoadIconW(NULL, IDI_APPLICATION);
    window_class.hIcon = g_icon;
    window_class.hIconSm = g_icon;

    if (!RegisterClassExW(&window_class)) {
        MessageBoxW(NULL, L"프림별 창을 등록하지 못했습니다.", APP_TITLE, MB_OK | MB_ICONERROR);
        FreeAnimationFrames(g_animation_frames);
        GdiplusShutdown(g_gdiplus_token);
        CloseHandle(g_mutex);
        return 1;
    }

    WNDCLASSEXW settings_class;
    ZeroMemory(&settings_class, sizeof(settings_class));
    settings_class.cbSize = sizeof(settings_class);
    settings_class.lpfnWndProc = SettingsWindowProcedure;
    settings_class.hInstance = instance;
    settings_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    settings_class.hIcon = g_icon;
    settings_class.hIconSm = g_icon;
    settings_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    settings_class.lpszClassName = L"PrimbyulSettingsWindow";
    RegisterClassExW(&settings_class);

    g_scale_percent = ReadIntSetting(L"Scale", 100);
    if (g_scale_percent != 75 && g_scale_percent != 100 && g_scale_percent != 135) {
        g_scale_percent = 100;
    }
    g_click_through = ReadIntSetting(L"ClickThrough", 0) != 0;
    g_topmost = ReadIntSetting(L"Topmost", 1) != 0;
    g_behavior_mode = ReadIntSetting(L"BehaviorMode", MODE_AUTO);
    if (g_behavior_mode != MODE_SIT && g_behavior_mode != MODE_QUIET) {
        g_behavior_mode = MODE_AUTO;
    }

    int width = MulDiv(CELL_WIDTH, g_scale_percent, 100);
    int height = MulDiv(CELL_HEIGHT, g_scale_percent, 100);
    RECT work_area;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    int x = ReadIntSetting(L"X", work_area.right - width - 24);
    int y = ReadIntSetting(L"Y", work_area.bottom - height - 24);
    if (x < work_area.left - width + 48 || x > work_area.right - 48) {
        x = work_area.right - width - 24;
    }
    if (y < work_area.top - height + 48 || y > work_area.bottom - 48) {
        y = work_area.bottom - height - 24;
    }

    if (!CreateFrameSurface(width, height)) {
        MessageBoxW(NULL, L"프림별 화면을 만들지 못했습니다.", APP_TITLE, MB_OK | MB_ICONERROR);
        FreeAnimationFrames(g_animation_frames);
        GdiplusShutdown(g_gdiplus_token);
        CloseHandle(g_mutex);
        return 1;
    }

    g_window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | (g_topmost ? WS_EX_TOPMOST : 0),
        window_class.lpszClassName,
        APP_TITLE,
        WS_POPUP,
        x,
        y,
        width,
        height,
        NULL,
        NULL,
        instance,
        NULL
    );

    if (!g_window) {
        MessageBoxW(NULL, L"프림별을 시작하지 못했습니다.", APP_TITLE, MB_OK | MB_ICONERROR);
        DestroyFrameSurface();
        FreeAnimationFrames(g_animation_frames);
        GdiplusShutdown(g_gdiplus_token);
        CloseHandle(g_mutex);
        return 1;
    }

    SetClickThrough(g_click_through);
    ShowWindow(g_window, SW_SHOWNOACTIVATE);
    SetTimer(g_window, TIMER_ANIMATION, TIMER_TICK_MS, NULL);
    ApplyBehaviorMode(g_behavior_mode);
    ClampPetToWorkArea();

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DestroyFrameSurface();
    FreeAnimationFrames(g_animation_frames);
    GdiplusShutdown(g_gdiplus_token);
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }
    return (int)message.wParam;
}
