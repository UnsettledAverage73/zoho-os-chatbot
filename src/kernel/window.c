#include "window.h"
#include "kmalloc.h"
#include "graphics.h"
#include "mouse.h"
#include "lock.h"
#include "string.h"
#include "klog.h"

#define DESKTOP_COLOR 0xFF112233
#define WINDOW_TITLE_HEIGHT 25
#define WINDOW_SHADOW_OFFSET 5
#define CURSOR_W 10
#define CURSOR_H 15

static window_t* window_list = NULL;
static uint32_t next_window_id = 1;
static spinlock_t window_lock;
static rect_t dirty_rects[WINDOW_MAX_DIRTY_RECTS];
static uint32_t dirty_count = 0;
static int32_t cursor_x = 0;
static int32_t cursor_y = 0;
static int cursor_initialized = 0;
static window_t* dragging_window = NULL;
static int32_t drag_off_x, drag_off_y;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t title_color;
    uint32_t bg_color;
    uint32_t* buffer;
} window_snapshot_t;

typedef struct {
    window_t* win;
    uint32_t local_x;
    uint32_t local_y;
    uint8_t in_title;
    uint8_t in_body;
} window_hit_t;

static rect_t make_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    rect_t rect = {x, y, w, h};
    return rect;
}

static rect_t rect_union(rect_t a, rect_t b) {
    int32_t x1 = (a.x < b.x) ? a.x : b.x;
    int32_t y1 = (a.y < b.y) ? a.y : b.y;
    int32_t x2 = (a.x + a.w > b.x + b.w) ? a.x + a.w : b.x + b.w;
    int32_t y2 = (a.y + a.h > b.y + b.h) ? a.y + a.h : b.y + b.h;
    return make_rect(x1, y1, x2 - x1, y2 - y1);
}

static int rect_intersect(rect_t a, rect_t b, rect_t* out) {
    int32_t x1 = (a.x > b.x) ? a.x : b.x;
    int32_t y1 = (a.y > b.y) ? a.y : b.y;
    int32_t x2 = (a.x + a.w < b.x + b.w) ? a.x + a.w : b.x + b.w;
    int32_t y2 = (a.y + a.h < b.y + b.h) ? a.y + a.h : b.y + b.h;

    if (x2 <= x1 || y2 <= y1) return 0;

    out->x = x1;
    out->y = y1;
    out->w = x2 - x1;
    out->h = y2 - y1;
    return 1;
}

static rect_t window_outer_rect(window_t* win) {
    return make_rect((int32_t)win->x,
                     (int32_t)win->y,
                     (int32_t)win->w + WINDOW_SHADOW_OFFSET,
                     (int32_t)win->h + WINDOW_SHADOW_OFFSET);
}

static rect_t cursor_rect(int32_t x, int32_t y) {
    return make_rect(x, y, CURSOR_W, CURSOR_H);
}

static rect_t screen_rect() {
    return make_rect(0, 0, (int32_t)graphics_get_width(), (int32_t)graphics_get_height());
}

static void free_window(window_t* win) {
    if (!win) return;
    if (win->buffer) kfree(win->buffer);
    kfree(win);
}

static void retain_window_locked(window_t* win) {
    if (!win) return;
    win->refcount++;
}

static void release_window_locked(window_t* win) {
    if (!win || win->refcount == 0) return;

    win->refcount--;
    if (win->destroyed && win->refcount == 0) {
        free_window(win);
    }
}

static int snapshot_window_list(window_t** held_windows, window_snapshot_t* snapshots, int max_count) {
    int count = 0;
    window_t* curr = window_list;
    while (curr && count < max_count) {
        retain_window_locked(curr);
        held_windows[count] = curr;
        snapshots[count].x = curr->x;
        snapshots[count].y = curr->y;
        snapshots[count].w = curr->w;
        snapshots[count].h = curr->h;
        snapshots[count].title_color = curr->title_color;
        snapshots[count].bg_color = curr->bg_color;
        snapshots[count].buffer = curr->buffer;
        count++;
        curr = curr->next;
    }
    return count;
}

static void release_window_refs_locked(window_t** held_windows, int count) {
    for (int i = 0; i < count; i++) {
        release_window_locked(held_windows[i]);
    }
}

static window_hit_t find_window_hit(window_t** held_windows,
                                    const window_snapshot_t* snapshots,
                                    int count,
                                    int32_t mx,
                                    int32_t my) {
    window_hit_t hit;
    hit.win = NULL;
    hit.local_x = 0;
    hit.local_y = 0;
    hit.in_title = 0;
    hit.in_body = 0;

    for (int i = 0; i < count; i++) {
        const window_snapshot_t* snap = &snapshots[i];
        if (mx < (int32_t)snap->x || mx >= (int32_t)(snap->x + snap->w)) continue;
        if (my < (int32_t)snap->y || my >= (int32_t)(snap->y + snap->h)) continue;

        hit.win = held_windows[i];
        hit.local_x = (uint32_t)(mx - (int32_t)snap->x);
        if (my < (int32_t)snap->y + WINDOW_TITLE_HEIGHT) {
            hit.in_title = 1;
            hit.local_y = (uint32_t)(my - (int32_t)snap->y);
        } else {
            hit.in_body = 1;
            hit.local_y = (uint32_t)(my - (int32_t)snap->y - WINDOW_TITLE_HEIGHT);
        }
        break;
    }

    return hit;
}

static void queue_dirty_rect(rect_t rect) {
    rect_t clipped;
    if (!rect_intersect(rect, screen_rect(), &clipped)) return;

    for (uint32_t i = 0; i < dirty_count; i++) {
        rect_t overlap;
        if (rect_intersect(dirty_rects[i], clipped, &overlap)) {
            dirty_rects[i] = rect_union(dirty_rects[i], clipped);
            return;
        }
    }

    if (dirty_count < WINDOW_MAX_DIRTY_RECTS) {
        dirty_rects[dirty_count++] = clipped;
        return;
    }

    dirty_rects[0] = rect_union(dirty_rects[0], clipped);
    dirty_count = 1;
}

static void draw_rect_clipped(rect_t clip, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    rect_t target = make_rect(x, y, w, h);
    rect_t intersection;
    if (!rect_intersect(clip, target, &intersection)) return;

    graphics_draw_rect((uint32_t)intersection.x,
                       (uint32_t)intersection.y,
                       (uint32_t)intersection.w,
                       (uint32_t)intersection.h,
                       color);
}

static void draw_window_buffer_clipped(rect_t clip, window_t* win) {
    if (!win->buffer) {
        draw_rect_clipped(clip,
                          (int32_t)win->x,
                          (int32_t)win->y + WINDOW_TITLE_HEIGHT,
                          (int32_t)win->w,
                          (int32_t)win->h - WINDOW_TITLE_HEIGHT,
                          win->bg_color);
        return;
    }

    rect_t body = make_rect((int32_t)win->x,
                            (int32_t)win->y + WINDOW_TITLE_HEIGHT,
                            (int32_t)win->w,
                            (int32_t)win->h - WINDOW_TITLE_HEIGHT);
    rect_t intersection;
    if (!rect_intersect(clip, body, &intersection)) return;

    int32_t src_x = intersection.x - (int32_t)win->x;
    int32_t src_y = intersection.y - ((int32_t)win->y + WINDOW_TITLE_HEIGHT);

    for (int32_t y = 0; y < intersection.h; y++) {
        for (int32_t x = 0; x < intersection.w; x++) {
            uint32_t color = win->buffer[(uint64_t)(src_y + y) * win->w + (src_x + x)];
            graphics_put_pixel((uint32_t)(intersection.x + x), (uint32_t)(intersection.y + y), color);
        }
    }
}

static void draw_window_clipped(rect_t clip, window_t* win) {
    draw_rect_clipped(clip,
                      (int32_t)win->x + WINDOW_SHADOW_OFFSET,
                      (int32_t)win->y + WINDOW_SHADOW_OFFSET,
                      (int32_t)win->w,
                      (int32_t)win->h,
                      0xAA000000);
    draw_window_buffer_clipped(clip, win);
    draw_rect_clipped(clip, (int32_t)win->x, (int32_t)win->y, (int32_t)win->w, WINDOW_TITLE_HEIGHT, win->title_color);
    draw_rect_clipped(clip, (int32_t)win->x, (int32_t)win->y, (int32_t)win->w, 1, 0xFFFFFFFF);
    draw_rect_clipped(clip, (int32_t)win->x, (int32_t)win->y + (int32_t)win->h - 1, (int32_t)win->w, 1, 0xFFFFFFFF);
    draw_rect_clipped(clip, (int32_t)win->x, (int32_t)win->y, 1, (int32_t)win->h, 0xFFFFFFFF);
    draw_rect_clipped(clip, (int32_t)win->x + (int32_t)win->w - 1, (int32_t)win->y, 1, (int32_t)win->h, 0xFFFFFFFF);
}

static void draw_window_snapshot_clipped(rect_t clip, const window_snapshot_t* win) {
    window_t temp;
    temp.id = 0;
    temp.x = win->x;
    temp.y = win->y;
    temp.w = win->w;
    temp.h = win->h;
    temp.title_color = win->title_color;
    temp.bg_color = win->bg_color;
    temp.buffer = win->buffer;
    temp.owner_pid = 0;
    temp.dirty = 0;
    temp.destroyed = 0;
    temp.refcount = 0;
    temp.event_head = 0;
    temp.event_tail = 0;
    temp.next = NULL;
    draw_window_clipped(clip, &temp);
}

void window_init() {
    window_list = NULL;
    spin_init(&window_lock);
    dirty_count = 0;
    cursor_initialized = 0;
    queue_dirty_rect(screen_rect());
}

window_t* window_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_color) {
    uint64_t flags = spin_lock_irqsave(&window_lock);
    window_t* win = kmalloc(sizeof(window_t));
    win->id = next_window_id++;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->bg_color = bg_color;
    win->title_color = 0xFF555555;
    win->buffer = NULL;
    win->owner_pid = 0;
    win->dirty = 1;
    win->destroyed = 0;
    win->refcount = 0;
    win->event_head = 0;
    win->event_tail = 0;

    win->next = window_list;
    window_list = win;
    queue_dirty_rect(window_outer_rect(win));
    spin_unlock_irqrestore(&window_lock, flags);
    return win;
}

void window_mark_dirty(window_t* win) {
    if (!win) return;
    uint64_t flags = spin_lock_irqsave(&window_lock);
    win->dirty = 1;
    queue_dirty_rect(window_outer_rect(win));
    spin_unlock_irqrestore(&window_lock, flags);
}

void window_mark_region_dirty(window_t* win, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!win) return;

    uint64_t flags = spin_lock_irqsave(&window_lock);
    win->dirty = 1;
    queue_dirty_rect(make_rect((int32_t)win->x + x,
                               (int32_t)win->y + WINDOW_TITLE_HEIGHT + y,
                               w,
                               h));
    spin_unlock_irqrestore(&window_lock, flags);
}

int window_needs_redraw() {
    return dirty_count != 0;
}

void window_push_event(window_t* win, gui_event_t event) {
    uint8_t next = (win->event_tail + 1) % 16;
    if (next == win->event_head) return;
    win->event_queue[win->event_tail] = event;
    win->event_tail = next;
}

window_t* window_get_by_id(uint32_t id) {
    uint64_t flags = spin_lock_irqsave(&window_lock);
    window_t* curr = window_list;
    while (curr) {
        if (curr->id == id) {
            if (curr->destroyed) {
                break;
            }
            spin_unlock_irqrestore(&window_lock, flags);
            return curr;
        }
        curr = curr->next;
    }
    spin_unlock_irqrestore(&window_lock, flags);
    return NULL;
}

void window_set_owner(window_t* win, uint64_t owner_pid) {
    if (!win) return;

    uint64_t flags = spin_lock_irqsave(&window_lock);
    if (!win->destroyed) {
        win->owner_pid = owner_pid;
    }
    spin_unlock_irqrestore(&window_lock, flags);
}

void window_set_buffer(window_t* win, uint32_t* buffer) {
    if (!win) return;

    uint64_t flags = spin_lock_irqsave(&window_lock);
    if (!win->destroyed) {
        win->buffer = buffer;
        win->dirty = 1;
        queue_dirty_rect(window_outer_rect(win));
    }
    spin_unlock_irqrestore(&window_lock, flags);
}

int window_pop_event(uint32_t id, gui_event_t* out_event) {
    if (!out_event) return -1;

    gui_event_t event;
    uint64_t flags = spin_lock_irqsave(&window_lock);
    window_t* curr = window_list;
    while (curr) {
        if (curr->id == id) {
            if (curr->destroyed) {
                spin_unlock_irqrestore(&window_lock, flags);
                return -1;
            }
            if (curr->event_head == curr->event_tail) {
                spin_unlock_irqrestore(&window_lock, flags);
                return 0;
            }

            event = curr->event_queue[curr->event_head];
            curr->event_head = (curr->event_head + 1) % 16;
            spin_unlock_irqrestore(&window_lock, flags);
            memcpy(out_event, &event, sizeof(gui_event_t));
            return 1;
        }
        curr = curr->next;
    }

    spin_unlock_irqrestore(&window_lock, flags);
    return -1;
}

void window_destroy_by_pid(uint64_t pid) {
    uint64_t flags = spin_lock_irqsave(&window_lock);
    window_t* curr = window_list;
    window_t* prev = NULL;

    while (curr) {
        if (curr->owner_pid == pid) {
            queue_dirty_rect(window_outer_rect(curr));

            if (prev) prev->next = curr->next;
            else window_list = curr->next;

            if (dragging_window == curr) {
                dragging_window = NULL;
            }

            window_t* to_free = curr;
            curr = curr->next;
            to_free->destroyed = 1;
            if (to_free->refcount == 0) {
                free_window(to_free);
            }
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
    spin_unlock_irqrestore(&window_lock, flags);
}

void window_update() {
    int32_t mx = mouse_get_x();
    int32_t my = mouse_get_y();
    uint8_t mb = mouse_get_buttons();
    window_t* held_windows[16];
    window_snapshot_t snapshots[16];
    window_t* drag_candidate;
    int count;
    int cursor_was_initialized;

    static uint8_t last_mb = 0;
    static int32_t last_mx = 0, last_my = 0;

    if (cursor_initialized &&
        dragging_window == NULL &&
        mx == last_mx &&
        my == last_my &&
        mb == last_mb) {
        return;
    }

    uint64_t flags = spin_lock_irqsave(&window_lock);
    cursor_was_initialized = cursor_initialized;
    if (!cursor_initialized) {
        cursor_initialized = 1;
        cursor_x = mx;
        cursor_y = my;
    }
    drag_candidate = dragging_window;
    count = snapshot_window_list(held_windows, snapshots, 16);
    spin_unlock_irqrestore(&window_lock, flags);

    window_hit_t press_hit;
    window_hit_t move_hit;
    press_hit.win = NULL;
    press_hit.local_x = 0;
    press_hit.local_y = 0;
    press_hit.in_title = 0;
    press_hit.in_body = 0;
    move_hit.win = NULL;
    move_hit.local_x = 0;
    move_hit.local_y = 0;
    move_hit.in_title = 0;
    move_hit.in_body = 0;
    if ((mb & 1) && !drag_candidate) {
        press_hit = find_window_hit(held_windows, snapshots, count, mx, my);
    }
    if (mx != last_mx || my != last_my) {
        move_hit = find_window_hit(held_windows, snapshots, count, mx, my);
    }

    flags = spin_lock_irqsave(&window_lock);

    if (!cursor_was_initialized) {
        queue_dirty_rect(cursor_rect(mx, my));
    }

    if (mb & 1) {
        if (!dragging_window) {
            if (press_hit.win && !press_hit.win->destroyed) {
                if (press_hit.in_title) {
                    dragging_window = press_hit.win;
                    drag_off_x = mx - (int32_t)press_hit.win->x;
                    drag_off_y = my - (int32_t)press_hit.win->y;

                    if (press_hit.win != window_list) {
                        rect_t old_rect = window_outer_rect(press_hit.win);
                        window_t* prev = window_list;
                        while (prev && prev->next != press_hit.win) prev = prev->next;
                        if (prev) {
                            prev->next = press_hit.win->next;
                            press_hit.win->next = window_list;
                            window_list = press_hit.win;
                            queue_dirty_rect(old_rect);
                            queue_dirty_rect(window_outer_rect(press_hit.win));
                        }
                    }
                } else if (press_hit.in_body && !(last_mb & 1)) {
                    gui_event_t ev = {
                        GUI_EVENT_MOUSE_CLICK,
                        (uint32_t)(mx - (int32_t)press_hit.win->x),
                        (uint32_t)(my - (int32_t)press_hit.win->y - WINDOW_TITLE_HEIGHT),
                        1
                    };
                    window_push_event(press_hit.win, ev);
                }
            }
        } else if (!dragging_window->destroyed) {
            rect_t old_rect = window_outer_rect(dragging_window);
            dragging_window->x = (uint32_t)(mx - drag_off_x);
            dragging_window->y = (uint32_t)(my - drag_off_y);
            dragging_window->dirty = 1;
            queue_dirty_rect(old_rect);
            queue_dirty_rect(window_outer_rect(dragging_window));
        } else {
            dragging_window = NULL;
        }
    } else {
        dragging_window = NULL;
    }

    if (mx != last_mx || my != last_my) {
        queue_dirty_rect(cursor_rect(cursor_x, cursor_y));
        queue_dirty_rect(cursor_rect(mx, my));

        if (move_hit.win && move_hit.in_body && !move_hit.win->destroyed) {
            gui_event_t ev = {
                GUI_EVENT_MOUSE_MOVE,
                (uint32_t)(mx - (int32_t)move_hit.win->x),
                (uint32_t)(my - (int32_t)move_hit.win->y - WINDOW_TITLE_HEIGHT),
                0
            };
            window_push_event(move_hit.win, ev);
        }
    }

    cursor_x = mx;
    cursor_y = my;
    last_mb = mb;
    last_mx = mx;
    last_my = my;
    release_window_refs_locked(held_windows, count);
    if (dragging_window && dragging_window->destroyed) {
        dragging_window = NULL;
    }
    spin_unlock_irqrestore(&window_lock, flags);
}

void window_draw_all() {
    window_t* held_windows[16];
    window_snapshot_t snapshots[16];
    rect_t redraw[WINDOW_MAX_DIRTY_RECTS];
    rect_t cursor;
    uint32_t redraw_count;
    int count = 0;

    uint64_t flags = spin_lock_irqsave(&window_lock);
    if (dirty_count == 0) {
        spin_unlock_irqrestore(&window_lock, flags);
        return;
    }

    redraw_count = dirty_count;
    for (uint32_t i = 0; i < redraw_count; i++) {
        redraw[i] = dirty_rects[i];
    }
    dirty_count = 0;

    count = snapshot_window_list(held_windows, snapshots, 16);

    cursor = cursor_rect(cursor_x, cursor_y);
    spin_unlock_irqrestore(&window_lock, flags);

    for (uint32_t i = 0; i < redraw_count; i++) {
        graphics_clear_rect(redraw[i].x, redraw[i].y, redraw[i].w, redraw[i].h, DESKTOP_COLOR);

        for (int j = count - 1; j >= 0; j--) {
            draw_window_snapshot_clipped(redraw[i], &snapshots[j]);
        }
    }

    for (uint32_t i = 0; i < redraw_count; i++) {
        rect_t intersection;
        if (rect_intersect(redraw[i], cursor, &intersection)) {
            graphics_draw_cursor((uint32_t)cursor.x, (uint32_t)cursor.y);
            break;
        }
    }

    graphics_swap_rects(redraw, redraw_count);
    flags = spin_lock_irqsave(&window_lock);
    release_window_refs_locked(held_windows, count);
    spin_unlock_irqrestore(&window_lock, flags);
}
