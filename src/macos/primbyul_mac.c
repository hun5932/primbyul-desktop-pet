#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Primbyul desktop pet for macOS.
 *
 * This file deliberately resolves AppKit and ServiceManagement at runtime.
 * That keeps the source cross-compilable with Zig on Windows/Linux while the
 * produced app still uses native AppKit objects on macOS.  Typed objc_msgSend
 * wrappers are centralized near the top; do not call objc_msgSend with an
 * unverified signature.  See docs/CODE-WALKTHROUGH.md and
 * docs/KNOWN-LIMITATIONS.md before modifying this platform layer.
 */

typedef void *id;
typedef void *Class;
typedef void *SEL;
typedef void *IMP;
typedef signed char ObjCBool;
typedef struct { double x, y; } NSPoint;
typedef struct { double width, height; } NSSize;
typedef struct { NSPoint origin; NSSize size; } NSRect;

enum {
    STATE_IDLE = 0,
    STATE_RUN_RIGHT,
    STATE_RUN_LEFT,
    STATE_WAG,
    STATE_PLAY,
    STATE_WATCH,
    STATE_SIT
};
enum { MODE_AUTO = 0, MODE_SIT = 1, MODE_QUIET = 2 };
enum { APPEARANCE_ADULT = 0, APPEARANCE_PUPPY = 1 };

static const int SEQ_IDLE[] = {0, 0, 0, 0};
static const int SEQ_RUN[] = {0, 1, 2, 3};
static const int SEQ_WAG[] = {0, 1, 2, 3};
static const int SEQ_PLAY[] = {0, 1, 2, 3, 2, 1, 0, 0};
static const int SEQ_WATCH[] = {0, 1, 2, 1, 3, 1, 0, 1};
static const int *SEQUENCES[] = {
    SEQ_IDLE, SEQ_RUN, SEQ_RUN, SEQ_WAG, SEQ_PLAY, SEQ_WATCH, SEQ_IDLE
};
static const int SEQUENCE_COUNTS[] = {4, 4, 4, 4, 8, 8, 4};
static const double FRAME_DELAYS[] = {0.14, 0.085, 0.085, 0.115, 0.12, 0.21, 0.14};
static const int LOOP_COUNTS[] = {1, 5, 5, 4, 2, 2, 1};
static const char *STATE_DIRECTORIES[] = {
    "idle", "run", "run-left", "wag", "play", "watch", "sit"
};

static void *g_msg;
static void *g_msg_stret;
static Class (*g_get_class)(const char *);
static SEL (*g_sel)(const char *);
static Class (*g_allocate_class_pair)(Class, const char *, size_t);
static void (*g_register_class_pair)(Class);
static ObjCBool (*g_add_method)(Class, SEL, IMP, const char *);

static id g_app;
static id g_controller;
static id g_window;
static id g_pet_view;
static id g_menu;
static id g_status_item;
static id g_frames[4];
static id g_show_item;
static id g_pause_item;
static id g_adult_item;
static id g_puppy_item;
static id g_auto_item;
static id g_quiet_item;
static id g_sit_item;
static id g_click_item;
static id g_top_item;
static id g_autostart_item;
static id g_small_item;
static id g_normal_item;
static id g_large_item;

static int g_state = STATE_IDLE;
static int g_mode = MODE_AUTO;
static int g_appearance = APPEARANCE_ADULT;
static int g_frame_index;
static int g_completed_loops;
static int g_blink_phase;
static int g_scale_percent = 100;
static ObjCBool g_paused;
static ObjCBool g_click_through;
static ObjCBool g_topmost = 1;
static ObjCBool g_visible = 1;
static double g_x;
static double g_y;
static double g_size = 220.0;
static double g_next_frame_at;
static double g_next_blink_at;
static double g_state_end_at;

#define S(name) g_sel(name)
#define C(name) g_get_class(name)
#define MSG_ID0(o, s) ((id (*)(id, SEL))g_msg)((o), (s))
#define MSG_ID1(o, s, a) ((id (*)(id, SEL, id))g_msg)((o), (s), (a))
#define MSG_ID2(o, s, a, b) ((id (*)(id, SEL, id, id))g_msg)((o), (s), (a), (b))
#define MSG_ID3(o, s, a, b, c) ((id (*)(id, SEL, id, id, id))g_msg)((o), (s), (a), (b), (c))
#define MSG_VOID0(o, s) ((void (*)(id, SEL))g_msg)((o), (s))
#define MSG_VOID1(o, s, a) ((void (*)(id, SEL, id))g_msg)((o), (s), (a))
#define MSG_VOID_BOOL(o, s, a) ((void (*)(id, SEL, ObjCBool))g_msg)((o), (s), (a))
#define MSG_VOID_LONG(o, s, a) ((void (*)(id, SEL, long))g_msg)((o), (s), (a))
#define MSG_VOID_ULONG(o, s, a) ((void (*)(id, SEL, unsigned long))g_msg)((o), (s), (a))
#define MSG_BOOL0(o, s) ((ObjCBool (*)(id, SEL))g_msg)((o), (s))
#define MSG_BOOL1(o, s, a) ((ObjCBool (*)(id, SEL, id))g_msg)((o), (s), (a))
#define MSG_LONG0(o, s) ((long (*)(id, SEL))g_msg)((o), (s))
#define MSG_LONG1(o, s, a) ((long (*)(id, SEL, id))g_msg)((o), (s), (a))
#define MSG_DOUBLE1(o, s, a) ((double (*)(id, SEL, id))g_msg)((o), (s), (a))

static id ns(const char *utf8) {
    return MSG_ID1((id)C("NSString"), S("stringWithUTF8String:"), (id)utf8);
}

static double now_seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

static NSRect msg_rect0(id object, SEL selector) {
    NSRect result;
#if defined(__x86_64__)
    ((void (*)(NSRect *, id, SEL))g_msg_stret)(&result, object, selector);
#else
    result = ((NSRect (*)(id, SEL))g_msg)(object, selector);
#endif
    return result;
}

static id msg_window_init(id object, NSRect rect) {
    return ((id (*)(id, SEL, NSRect, unsigned long, unsigned long, ObjCBool))g_msg)(
        object,
        S("initWithContentRect:styleMask:backing:defer:"),
        rect,
        1ul << 7,
        2ul,
        0
    );
}

static void msg_set_frame(id object, NSRect rect, ObjCBool display) {
    ((void (*)(id, SEL, NSRect, ObjCBool))g_msg)(
        object, S("setFrame:display:"), rect, display
    );
}

static void msg_set_frame_origin(id object, NSPoint point) {
    ((void (*)(id, SEL, NSPoint))g_msg)(object, S("setFrameOrigin:"), point);
}

static id defaults(void) {
    return MSG_ID0((id)C("NSUserDefaults"), S("standardUserDefaults"));
}

static void save_bool(const char *key, ObjCBool value) {
    ((void (*)(id, SEL, ObjCBool, id))g_msg)(
        defaults(), S("setBool:forKey:"), value, ns(key)
    );
}

static void save_integer(const char *key, long value) {
    ((void (*)(id, SEL, long, id))g_msg)(
        defaults(), S("setInteger:forKey:"), value, ns(key)
    );
}

static void save_double(const char *key, double value) {
    ((void (*)(id, SEL, double, id))g_msg)(
        defaults(), S("setDouble:forKey:"), value, ns(key)
    );
}

static ObjCBool read_bool(const char *key, ObjCBool fallback) {
    id user_defaults = defaults();
    id value = MSG_ID1(user_defaults, S("objectForKey:"), ns(key));
    if (!value) return fallback;
    return MSG_BOOL1(user_defaults, S("boolForKey:"), ns(key));
}

static long read_integer(const char *key, long fallback) {
    id user_defaults = defaults();
    id value = MSG_ID1(user_defaults, S("objectForKey:"), ns(key));
    if (!value) return fallback;
    return MSG_LONG1(user_defaults, S("integerForKey:"), ns(key));
}

static double read_double(const char *key, double fallback) {
    id user_defaults = defaults();
    id value = MSG_ID1(user_defaults, S("objectForKey:"), ns(key));
    if (!value) return fallback;
    return MSG_DOUBLE1(user_defaults, S("doubleForKey:"), ns(key));
}

static void save_settings(void) {
    save_double("PetX", g_x);
    save_double("PetY", g_y);
    save_integer("Scale", g_scale_percent);
    save_integer("BehaviorMode", g_mode);
    save_integer("AppearanceMode", g_appearance);
    save_bool("ClickThrough", g_click_through);
    save_bool("Topmost", g_topmost);
    MSG_VOID0(defaults(), S("synchronize"));
}

static void show_alert(const char *title, const char *message) {
    id alert = MSG_ID0(MSG_ID0((id)C("NSAlert"), S("alloc")), S("init"));
    MSG_VOID1(alert, S("setMessageText:"), ns(title));
    MSG_VOID1(alert, S("setInformativeText:"), ns(message));
    MSG_ID1(alert, S("addButtonWithTitle:"), ns("확인"));
    MSG_LONG0(alert, S("runModal"));
    MSG_VOID0(alert, S("release"));
}

static NSRect visible_frame(void) {
    id screen = MSG_ID0(g_window, S("screen"));
    if (!screen) screen = MSG_ID0((id)C("NSScreen"), S("mainScreen"));
    return msg_rect0(screen, S("visibleFrame"));
}

static void clamp_position(void) {
    NSRect frame = visible_frame();
    if (g_x < frame.origin.x) g_x = frame.origin.x;
    if (g_y < frame.origin.y) g_y = frame.origin.y;
    if (g_x + g_size > frame.origin.x + frame.size.width) {
        g_x = frame.origin.x + frame.size.width - g_size;
    }
    if (g_y + g_size > frame.origin.y + frame.size.height) {
        g_y = frame.origin.y + frame.size.height - g_size;
    }
    msg_set_frame_origin(g_window, (NSPoint){g_x, g_y});
}

static void set_menu_check(id item, int checked) {
    if (item) MSG_VOID_LONG(item, S("setState:"), checked ? 1 : 0);
}

static ObjCBool autostart_enabled(void) {
    Class service_class = C("SMAppService");
    if (!service_class) return 0;
    id service = MSG_ID0((id)service_class, S("mainAppService"));
    return service && MSG_LONG0(service, S("status")) == 1;
}

static void update_menu(void) {
    if (!g_menu) return;
    MSG_VOID1(g_show_item, S("setTitle:"), ns(
        g_visible ? "프림별 잠시 숨기기" : "프림별 보이기"
    ));
    MSG_VOID1(g_pause_item, S("setTitle:"), ns(
        g_paused ? "움직임 다시 시작" : "움직임 일시정지"
    ));
    set_menu_check(g_adult_item, g_appearance == APPEARANCE_ADULT);
    set_menu_check(g_puppy_item, g_appearance == APPEARANCE_PUPPY);
    set_menu_check(g_auto_item, g_mode == MODE_AUTO);
    set_menu_check(g_quiet_item, g_mode == MODE_QUIET);
    set_menu_check(g_sit_item, g_mode == MODE_SIT);
    set_menu_check(g_click_item, g_click_through);
    set_menu_check(g_top_item, g_topmost);
    set_menu_check(g_autostart_item, autostart_enabled());
    set_menu_check(g_small_item, g_scale_percent == 75);
    set_menu_check(g_normal_item, g_scale_percent == 100);
    set_menu_check(g_large_item, g_scale_percent == 135);
}

static void release_frames(id frames[4]) {
    for (int index = 0; index < 4; ++index) {
        if (frames[index]) {
            MSG_VOID0(frames[index], S("release"));
            frames[index] = NULL;
        }
    }
}

static ObjCBool load_frames(int appearance, int state) {
    id loaded[4] = {NULL, NULL, NULL, NULL};
    id bundle = MSG_ID0((id)C("NSBundle"), S("mainBundle"));
    char directory[128];
    snprintf(
        directory,
        sizeof(directory),
        "frames/%s/%s",
        appearance == APPEARANCE_PUPPY ? "puppy" : "adult",
        STATE_DIRECTORIES[state]
    );
    for (int key = 0; key < 4; ++key) {
        char name[8];
        snprintf(name, sizeof(name), "%d", key);
        id path = MSG_ID3(
            bundle,
            S("pathForResource:ofType:inDirectory:"),
            ns(name),
            ns("png"),
            ns(directory)
        );
        if (!path) {
            release_frames(loaded);
            return 0;
        }
        loaded[key] = MSG_ID1(
            MSG_ID0((id)C("NSImage"), S("alloc")),
            S("initWithContentsOfFile:"),
            path
        );
        if (!loaded[key]) {
            release_frames(loaded);
            return 0;
        }
    }
    release_frames(g_frames);
    memcpy(g_frames, loaded, sizeof(loaded));
    return 1;
}

static void display_current_frame(void) {
    int key = SEQUENCES[g_state][g_frame_index % SEQUENCE_COUNTS[g_state]];
    if ((g_state == STATE_IDLE || g_state == STATE_SIT) && g_blink_phase > 0) {
        key = g_blink_phase == 2 ? 2 : 1;
    }
    if (key < 0 || key > 3) key = 0;
    MSG_VOID1(g_pet_view, S("setImage:"), g_frames[key]);
}

static void set_state(int state) {
    if (state < STATE_IDLE || state > STATE_SIT) state = STATE_IDLE;
    if (state != g_state && !load_frames(g_appearance, state)) {
        show_alert("프림별", "동작 이미지를 불러오지 못했습니다.");
        return;
    }
    g_state = state;
    g_frame_index = 0;
    g_completed_loops = 0;
    g_blink_phase = 0;
    double now = now_seconds();
    g_next_frame_at = now + FRAME_DELAYS[g_state];
    g_next_blink_at = now + 3.0 + (double)(rand() % 5001) / 1000.0;
    if (state == STATE_IDLE) {
        g_state_end_at = now + (
            g_mode == MODE_QUIET
                ? 9.0 + (double)(rand() % 7001) / 1000.0
                : 5.0 + (double)(rand() % 6001) / 1000.0
        );
    } else {
        g_state_end_at = 0.0;
    }
    display_current_frame();
}

static int choose_run_state(void) {
    NSRect frame = visible_frame();
    double center = g_x + g_size / 2.0;
    double work_center = frame.origin.x + frame.size.width / 2.0;
    if (center < work_center) return STATE_RUN_RIGHT;
    if (center > work_center) return STATE_RUN_LEFT;
    return (rand() & 1) ? STATE_RUN_RIGHT : STATE_RUN_LEFT;
}

static int select_next_state(void) {
    int roll = rand() % 100;
    if (g_mode == MODE_QUIET) {
        if (roll < 70) return STATE_IDLE;
        if (roll < 86) return STATE_WATCH;
        if (roll < 96) return STATE_WAG;
        return STATE_PLAY;
    }
    if (roll < 55) return STATE_IDLE;
    if (roll < 67) return choose_run_state();
    if (roll < 82) return STATE_WAG;
    if (roll < 91) return STATE_PLAY;
    return STATE_WATCH;
}

static void move_run_step(void) {
    if (g_state != STATE_RUN_RIGHT && g_state != STATE_RUN_LEFT) return;
    NSRect frame = visible_frame();
    double step = 7.0 * (double)g_scale_percent / 100.0;
    if (step < 4.0) step = 4.0;
    if (g_state == STATE_RUN_LEFT) step = -step;
    g_x += step;
    if (g_x <= frame.origin.x) {
        g_x = frame.origin.x;
        if (load_frames(g_appearance, STATE_RUN_RIGHT)) {
            g_state = STATE_RUN_RIGHT;
        }
    } else if (g_x + g_size >= frame.origin.x + frame.size.width) {
        g_x = frame.origin.x + frame.size.width - g_size;
        if (load_frames(g_appearance, STATE_RUN_LEFT)) {
            g_state = STATE_RUN_LEFT;
        }
    }
    msg_set_frame_origin(g_window, (NSPoint){g_x, g_y});
}

static void apply_mode(int mode) {
    g_mode = mode == MODE_SIT ? MODE_SIT :
        mode == MODE_QUIET ? MODE_QUIET : MODE_AUTO;
    set_state(g_mode == MODE_SIT ? STATE_SIT : STATE_IDLE);
    save_settings();
    update_menu();
}

static void apply_appearance(int appearance) {
    int normalized = appearance == APPEARANCE_PUPPY
        ? APPEARANCE_PUPPY : APPEARANCE_ADULT;
    if (normalized != g_appearance) {
        if (!load_frames(normalized, g_state)) {
            show_alert("프림별", "선택한 프림 이미지를 불러오지 못했습니다.");
            return;
        }
        g_appearance = normalized;
        display_current_frame();
        save_settings();
        update_menu();
    }
}

static void set_scale(int percent) {
    g_scale_percent = percent;
    g_size = 220.0 * (double)percent / 100.0;
    clamp_position();
    msg_set_frame(
        g_window,
        (NSRect){{g_x, g_y}, {g_size, g_size}},
        1
    );
    ((void (*)(id, SEL, NSRect))g_msg)(
        g_pet_view, S("setFrame:"), (NSRect){{0, 0}, {g_size, g_size}}
    );
    save_settings();
    update_menu();
}

static void tick_blink(double now) {
    if (now < g_next_blink_at) return;
    if (g_blink_phase == 0) {
        g_blink_phase = 1;
        g_next_blink_at = now + 0.07;
    } else if (g_blink_phase == 1) {
        g_blink_phase = 2;
        g_next_blink_at = now + 0.09;
    } else if (g_blink_phase == 2) {
        g_blink_phase = 3;
        g_next_blink_at = now + 0.07;
    } else {
        g_blink_phase = 0;
        g_next_blink_at = now + 3.0 + (double)(rand() % 5001) / 1000.0;
    }
    display_current_frame();
}

static void timer_tick(id self, SEL command, id timer) {
    (void)self;
    (void)command;
    (void)timer;
    if (g_paused) return;
    double now = now_seconds();
    if (g_state == STATE_IDLE || g_state == STATE_SIT) {
        tick_blink(now);
        if (
            g_state == STATE_IDLE &&
            g_state_end_at > 0.0 &&
            now >= g_state_end_at &&
            g_blink_phase == 0
        ) {
            set_state(select_next_state());
        }
        return;
    }
    if (now < g_next_frame_at) return;
    g_next_frame_at = now + FRAME_DELAYS[g_state];
    ++g_frame_index;
    if (g_frame_index >= SEQUENCE_COUNTS[g_state]) {
        g_frame_index = 0;
        ++g_completed_loops;
        if (g_completed_loops >= LOOP_COUNTS[g_state]) {
            set_state(g_mode == MODE_SIT ? STATE_SIT : select_next_state());
            return;
        }
    }
    move_run_step();
    display_current_frame();
}

static void action_show(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    if (g_visible) {
        MSG_VOID1(g_window, S("orderOut:"), NULL);
        g_visible = 0;
    } else {
        MSG_VOID0(g_window, S("orderFrontRegardless"));
        g_visible = 1;
    }
    update_menu();
}

static void action_pause(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    g_paused = !g_paused;
    if (!g_paused) {
        double now = now_seconds();
        g_next_frame_at = now + FRAME_DELAYS[g_state];
        if (g_blink_phase == 0) {
            g_next_blink_at = now + 3.0 + (double)(rand() % 5001) / 1000.0;
        }
    }
    update_menu();
}

static void action_run(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_state(choose_run_state());
}
static void action_wag(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_state(STATE_WAG);
}
static void action_play(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_state(STATE_PLAY);
}
static void action_watch(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_state(STATE_WATCH);
}
static void action_auto(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; apply_mode(MODE_AUTO);
}
static void action_quiet(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; apply_mode(MODE_QUIET);
}
static void action_sit(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; apply_mode(MODE_SIT);
}
static void action_adult(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; apply_appearance(APPEARANCE_ADULT);
}
static void action_puppy(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; apply_appearance(APPEARANCE_PUPPY);
}
static void action_small(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_scale(75);
}
static void action_normal(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_scale(100);
}
static void action_large(id s, SEL c, id sender) {
    (void)s; (void)c; (void)sender; set_scale(135);
}

static void action_click_through(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    g_click_through = !g_click_through;
    MSG_VOID_BOOL(g_window, S("setIgnoresMouseEvents:"), g_click_through);
    save_settings();
    update_menu();
}

static void action_topmost(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    g_topmost = !g_topmost;
    MSG_VOID_LONG(g_window, S("setLevel:"), g_topmost ? 3 : 0);
    save_settings();
    update_menu();
}

static void action_reset(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    NSRect frame = visible_frame();
    g_x = frame.origin.x + frame.size.width - g_size - 24.0;
    g_y = frame.origin.y + 24.0;
    clamp_position();
    save_settings();
}

static void action_autostart(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    Class service_class = C("SMAppService");
    if (!service_class) {
        show_alert(
            "자동 실행",
            "이 기능은 macOS 13 Ventura 이상에서 사용할 수 있습니다."
        );
        return;
    }
    id service = MSG_ID0((id)service_class, S("mainAppService"));
    id error = NULL;
    ObjCBool success;
    if (autostart_enabled()) {
        success = ((ObjCBool (*)(id, SEL, id *))g_msg)(
            service, S("unregisterAndReturnError:"), &error
        );
    } else {
        success = ((ObjCBool (*)(id, SEL, id *))g_msg)(
            service, S("registerAndReturnError:"), &error
        );
    }
    if (!success) {
        const char *detail = "앱을 Applications 폴더로 옮긴 뒤 다시 시도해 주세요.";
        show_alert("자동 실행 설정 실패", detail);
    } else if (MSG_LONG0(service, S("status")) == 2) {
        show_alert(
            "자동 실행 승인 필요",
            "시스템 설정 > 일반 > 로그인 항목에서 Primbyul을 허용해 주세요."
        );
    }
    update_menu();
}

static void action_quit(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    save_settings();
    MSG_VOID1(g_app, S("terminate:"), NULL);
}

static void action_remove_and_quit(id self, SEL command, id sender) {
    (void)self; (void)command; (void)sender;
    Class service_class = C("SMAppService");
    if (service_class) {
        id service = MSG_ID0((id)service_class, S("mainAppService"));
        if (service && MSG_LONG0(service, S("status")) == 1) {
            id error = NULL;
            ((ObjCBool (*)(id, SEL, id *))g_msg)(
                service, S("unregisterAndReturnError:"), &error
            );
        }
    }
    id bundle = MSG_ID0((id)C("NSBundle"), S("mainBundle"));
    id identifier = MSG_ID0(bundle, S("bundleIdentifier"));
    MSG_VOID1(defaults(), S("removePersistentDomainForName:"), identifier);
    show_alert(
        "설정 제거 완료",
        "자동 실행과 저장된 설정을 제거했습니다. 종료 후 Primbyul.app을 삭제하면 완전히 제거됩니다."
    );
    MSG_VOID1(g_app, S("terminate:"), NULL);
}

static ObjCBool pet_accepts_first_mouse(id self, SEL command, id event) {
    (void)self; (void)command; (void)event; return 1;
}

static ObjCBool pet_mouse_moves_window(id self, SEL command) {
    (void)self; (void)command; return 1;
}

static id pet_hit_test(id self, SEL command, NSPoint point) {
    (void)command;
    id image = MSG_ID0(self, S("image"));
    if (!image || g_size <= 0.0) return self;
    id representations = MSG_ID0(image, S("representations"));
    if (!representations || MSG_LONG0(representations, S("count")) < 1) return self;
    id representation = ((id (*)(id, SEL, unsigned long))g_msg)(
        representations, S("objectAtIndex:"), 0ul
    );
    long width = MSG_LONG0(representation, S("pixelsWide"));
    long height = MSG_LONG0(representation, S("pixelsHigh"));
    if (width <= 0 || height <= 0) return self;
    long x = (long)(point.x * (double)width / g_size);
    long y = (long)(point.y * (double)height / g_size);
    if (x < 0 || y < 0 || x >= width || y >= height) return NULL;
    id color = ((id (*)(id, SEL, long, long))g_msg)(
        representation, S("colorAtX:y:"), x, y
    );
    if (!color) return NULL;
    double alpha = ((double (*)(id, SEL))g_msg)(color, S("alphaComponent"));
    return alpha < 0.07 ? NULL : self;
}

static void pet_mouse_down(id self, SEL command, id event) {
    (void)command;
    long click_count = MSG_LONG0(event, S("clickCount"));
    if (click_count >= 2) {
        set_state(STATE_WAG);
        return;
    }
    id window = MSG_ID0(self, S("window"));
    MSG_VOID1(window, S("performWindowDragWithEvent:"), event);
    NSRect frame = msg_rect0(window, S("frame"));
    g_x = frame.origin.x;
    g_y = frame.origin.y;
    clamp_position();
    save_settings();
}

static void pet_right_mouse_down(id self, SEL command, id event) {
    (void)command;
    ((void (*)(id, SEL, id, id, id))g_msg)(
        (id)C("NSMenu"),
        S("popUpContextMenu:withEvent:forView:"),
        g_menu,
        event,
        self
    );
}

static id add_item(id menu, const char *title, const char *action) {
    id item = MSG_ID3(
        MSG_ID0((id)C("NSMenuItem"), S("alloc")),
        S("initWithTitle:action:keyEquivalent:"),
        ns(title),
        action ? (id)S(action) : NULL,
        ns("")
    );
    if (action) MSG_VOID1(item, S("setTarget:"), g_controller);
    MSG_VOID1(menu, S("addItem:"), item);
    MSG_VOID0(item, S("autorelease"));
    return item;
}

static id add_submenu(id menu, const char *title) {
    id item = add_item(menu, title, NULL);
    id submenu = MSG_ID1(
        MSG_ID0((id)C("NSMenu"), S("alloc")),
        S("initWithTitle:"),
        ns(title)
    );
    ((void (*)(id, SEL, id, id))g_msg)(
        menu, S("setSubmenu:forItem:"), submenu, item
    );
    MSG_VOID0(submenu, S("autorelease"));
    return submenu;
}

static void add_separator(id menu) {
    MSG_VOID1(
        menu,
        S("addItem:"),
        MSG_ID0((id)C("NSMenuItem"), S("separatorItem"))
    );
}

static void build_menu(void) {
    g_menu = MSG_ID1(
        MSG_ID0((id)C("NSMenu"), S("alloc")),
        S("initWithTitle:"),
        ns("프림별")
    );
    MSG_VOID_BOOL(g_menu, S("setAutoenablesItems:"), 0);
    add_item(g_menu, "프림별 v1.5", NULL);
    add_separator(g_menu);
    g_show_item = add_item(g_menu, "프림별 잠시 숨기기", "actionShow:");
    g_pause_item = add_item(g_menu, "움직임 일시정지", "actionPause:");
    add_separator(g_menu);
    add_item(g_menu, "뛰어다니기", "actionRun:");
    add_item(g_menu, "꼬리 흔들기", "actionWag:");
    add_item(g_menu, "장난치기", "actionPlay:");
    add_item(g_menu, "옆에서 지켜보기", "actionWatch:");

    id appearance = add_submenu(g_menu, "프림 모습");
    g_adult_item = add_item(appearance, "성견 프림", "actionAdult:");
    g_puppy_item = add_item(appearance, "어릴 때 프림", "actionPuppy:");

    id behavior = add_submenu(g_menu, "동작 모드");
    g_auto_item = add_item(behavior, "자동으로 놀기", "actionAuto:");
    g_quiet_item = add_item(behavior, "조용히 지켜보기", "actionQuiet:");
    g_sit_item = add_item(behavior, "얌전히 앉아 있기", "actionSit:");

    id size_menu = add_submenu(g_menu, "크기");
    g_small_item = add_item(size_menu, "작게 (75%)", "actionSmall:");
    g_normal_item = add_item(size_menu, "보통 (100%)", "actionNormal:");
    g_large_item = add_item(size_menu, "크게 (135%)", "actionLarge:");

    add_separator(g_menu);
    g_click_item = add_item(g_menu, "마우스 클릭 통과", "actionClickThrough:");
    g_top_item = add_item(g_menu, "항상 위에 표시", "actionTopmost:");
    g_autostart_item = add_item(g_menu, "Mac 로그인 시 자동 실행", "actionAutostart:");
    add_item(g_menu, "오른쪽 아래로 이동", "actionReset:");
    add_separator(g_menu);
    add_item(g_menu, "설정 제거 후 종료", "actionRemoveAndQuit:");
    add_item(g_menu, "종료", "actionQuit:");
    update_menu();
}

static ObjCBool register_runtime_classes(void) {
    Class object = C("NSObject");
    Class controller = g_allocate_class_pair(object, "PrimbyulController", 0);
    if (!controller) controller = C("PrimbyulController");
    if (!controller) return 0;

    struct Method {
        const char *name;
        IMP implementation;
        const char *types;
    };
    const struct Method controller_methods[] = {
        {"tick:", (IMP)timer_tick, "v@:@"},
        {"actionShow:", (IMP)action_show, "v@:@"},
        {"actionPause:", (IMP)action_pause, "v@:@"},
        {"actionRun:", (IMP)action_run, "v@:@"},
        {"actionWag:", (IMP)action_wag, "v@:@"},
        {"actionPlay:", (IMP)action_play, "v@:@"},
        {"actionWatch:", (IMP)action_watch, "v@:@"},
        {"actionAuto:", (IMP)action_auto, "v@:@"},
        {"actionQuiet:", (IMP)action_quiet, "v@:@"},
        {"actionSit:", (IMP)action_sit, "v@:@"},
        {"actionAdult:", (IMP)action_adult, "v@:@"},
        {"actionPuppy:", (IMP)action_puppy, "v@:@"},
        {"actionSmall:", (IMP)action_small, "v@:@"},
        {"actionNormal:", (IMP)action_normal, "v@:@"},
        {"actionLarge:", (IMP)action_large, "v@:@"},
        {"actionClickThrough:", (IMP)action_click_through, "v@:@"},
        {"actionTopmost:", (IMP)action_topmost, "v@:@"},
        {"actionReset:", (IMP)action_reset, "v@:@"},
        {"actionAutostart:", (IMP)action_autostart, "v@:@"},
        {"actionRemoveAndQuit:", (IMP)action_remove_and_quit, "v@:@"},
        {"actionQuit:", (IMP)action_quit, "v@:@"}
    };
    for (size_t index = 0; index < sizeof(controller_methods) / sizeof(controller_methods[0]); ++index) {
        g_add_method(
            controller,
            S(controller_methods[index].name),
            controller_methods[index].implementation,
            controller_methods[index].types
        );
    }
    g_register_class_pair(controller);

    Class image_view = C("NSImageView");
    Class pet_view = g_allocate_class_pair(image_view, "PrimbyulPetView", 0);
    if (!pet_view) pet_view = C("PrimbyulPetView");
    if (!pet_view) return 0;
    g_add_method(pet_view, S("acceptsFirstMouse:"), (IMP)pet_accepts_first_mouse, "c@:@");
    g_add_method(pet_view, S("mouseDownCanMoveWindow"), (IMP)pet_mouse_moves_window, "c@:");
    g_add_method(pet_view, S("hitTest:"), (IMP)pet_hit_test, "@@:{CGPoint=dd}");
    g_add_method(pet_view, S("mouseDown:"), (IMP)pet_mouse_down, "v@:@");
    g_add_method(pet_view, S("rightMouseDown:"), (IMP)pet_right_mouse_down, "v@:@");
    g_register_class_pair(pet_view);
    return 1;
}

static void load_preferences(void) {
    g_scale_percent = (int)read_integer("Scale", 100);
    if (g_scale_percent != 75 && g_scale_percent != 100 && g_scale_percent != 135) {
        g_scale_percent = 100;
    }
    g_size = 220.0 * (double)g_scale_percent / 100.0;
    g_mode = (int)read_integer("BehaviorMode", MODE_AUTO);
    if (g_mode != MODE_SIT && g_mode != MODE_QUIET) g_mode = MODE_AUTO;
    g_appearance = (int)read_integer("AppearanceMode", APPEARANCE_ADULT);
    if (g_appearance != APPEARANCE_PUPPY) g_appearance = APPEARANCE_ADULT;
    g_click_through = read_bool("ClickThrough", 0);
    g_topmost = read_bool("Topmost", 1);
}

static ObjCBool initialize_objc(void) {
    if (!dlopen(
        "/System/Library/Frameworks/AppKit.framework/AppKit",
        RTLD_LAZY | RTLD_GLOBAL
    )) {
        return 0;
    }
    dlopen(
        "/System/Library/Frameworks/ServiceManagement.framework/ServiceManagement",
        RTLD_LAZY | RTLD_GLOBAL
    );
    g_msg = dlsym(RTLD_DEFAULT, "objc_msgSend");
    g_msg_stret = dlsym(RTLD_DEFAULT, "objc_msgSend_stret");
    g_get_class = dlsym(RTLD_DEFAULT, "objc_getClass");
    g_sel = dlsym(RTLD_DEFAULT, "sel_registerName");
    g_allocate_class_pair = dlsym(RTLD_DEFAULT, "objc_allocateClassPair");
    g_register_class_pair = dlsym(RTLD_DEFAULT, "objc_registerClassPair");
    g_add_method = dlsym(RTLD_DEFAULT, "class_addMethod");
    ObjCBool ready = g_msg && g_get_class && g_sel && g_allocate_class_pair
        && g_register_class_pair && g_add_method;
#if defined(__x86_64__)
    ready = ready && g_msg_stret;
#endif
    return ready;
}

int main(void) {
    srand((unsigned int)time(NULL));
    if (!initialize_objc()) {
        fprintf(stderr, "Primbyul: AppKit could not be loaded.\\n");
        return 1;
    }
    id pool = MSG_ID0(MSG_ID0((id)C("NSAutoreleasePool"), S("alloc")), S("init"));
    if (!register_runtime_classes()) {
        fprintf(stderr, "Primbyul: runtime classes could not be registered.\\n");
        return 1;
    }

    g_app = MSG_ID0((id)C("NSApplication"), S("sharedApplication"));
    ((ObjCBool (*)(id, SEL, long))g_msg)(
        g_app, S("setActivationPolicy:"), 1
    );
    g_controller = MSG_ID0(
        MSG_ID0((id)C("PrimbyulController"), S("alloc")),
        S("init")
    );
    load_preferences();

    id main_screen = MSG_ID0((id)C("NSScreen"), S("mainScreen"));
    NSRect work = msg_rect0(main_screen, S("visibleFrame"));
    double fallback_x = work.origin.x + work.size.width - g_size - 24.0;
    double fallback_y = work.origin.y + 24.0;
    g_x = read_double("PetX", fallback_x);
    g_y = read_double("PetY", fallback_y);

    g_window = msg_window_init(
        MSG_ID0((id)C("NSPanel"), S("alloc")),
        (NSRect){{g_x, g_y}, {g_size, g_size}}
    );
    if (!g_window) return 1;
    MSG_VOID_BOOL(g_window, S("setOpaque:"), 0);
    MSG_VOID_BOOL(g_window, S("setHasShadow:"), 0);
    MSG_VOID_BOOL(g_window, S("setHidesOnDeactivate:"), 0);
    MSG_VOID_BOOL(g_window, S("setMovableByWindowBackground:"), 1);
    MSG_VOID_BOOL(g_window, S("setIgnoresMouseEvents:"), g_click_through);
    MSG_VOID_ULONG(g_window, S("setCollectionBehavior:"), (1ul << 0) | (1ul << 8));
    MSG_VOID_LONG(g_window, S("setLevel:"), g_topmost ? 3 : 0);
    MSG_VOID1(
        g_window,
        S("setBackgroundColor:"),
        MSG_ID0((id)C("NSColor"), S("clearColor"))
    );

    g_pet_view = MSG_ID0(
        MSG_ID0((id)C("PrimbyulPetView"), S("alloc")),
        S("init")
    );
    ((void (*)(id, SEL, NSRect))g_msg)(
        g_pet_view, S("setFrame:"), (NSRect){{0, 0}, {g_size, g_size}}
    );
    MSG_VOID_LONG(g_pet_view, S("setImageScaling:"), 3);
    MSG_VOID1(g_window, S("setContentView:"), g_pet_view);

    if (!load_frames(g_appearance, STATE_IDLE)) {
        show_alert("프림별", "내장된 프림별 이미지를 불러오지 못했습니다.");
        return 1;
    }
    clamp_position();
    build_menu();

    id status_bar = MSG_ID0((id)C("NSStatusBar"), S("systemStatusBar"));
    g_status_item = ((id (*)(id, SEL, double))g_msg)(
        status_bar, S("statusItemWithLength:"), -1.0
    );
    id status_button = MSG_ID0(g_status_item, S("button"));
    id bundle = MSG_ID0((id)C("NSBundle"), S("mainBundle"));
    id icon_path = MSG_ID2(
        bundle,
        S("pathForResource:ofType:"),
        ns("primbyul-status-icon"),
        ns("png")
    );
    id status_image = MSG_ID1(
        MSG_ID0((id)C("NSImage"), S("alloc")),
        S("initWithContentsOfFile:"),
        icon_path
    );
    MSG_VOID1(status_button, S("setImage:"), status_image);
    MSG_VOID1(status_button, S("setToolTip:"), ns("프림별 데스크톱 펫"));
    MSG_VOID1(g_status_item, S("setMenu:"), g_menu);
    MSG_VOID0(status_image, S("release"));

    set_state(g_mode == MODE_SIT ? STATE_SIT : STATE_IDLE);
    MSG_VOID0(g_window, S("orderFrontRegardless"));
    ((id (*)(id, SEL, double, id, SEL, id, ObjCBool))g_msg)(
        (id)C("NSTimer"),
        S("scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:"),
        0.025,
        g_controller,
        S("tick:"),
        NULL,
        1
    );
    MSG_VOID0(g_app, S("finishLaunching"));
    MSG_VOID0(g_app, S("run"));

    release_frames(g_frames);
    MSG_VOID0(g_pet_view, S("release"));
    MSG_VOID0(g_window, S("release"));
    MSG_VOID0(g_controller, S("release"));
    MSG_VOID0(g_menu, S("release"));
    MSG_VOID0(pool, S("drain"));
    return 0;
}
