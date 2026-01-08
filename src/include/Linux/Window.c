#include "Window.h"
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <X11/Xatom.h>

static Display* dis = NULL;
static int screen;
static Window lockWin = 0;
static Window timerWin = 0;
static Pixmap timerPixmap = 0;
static Pixmap lockPixmap = 0;
static GC lockGC = 0;
static GC timerGC = 0;
static Window trayWin = 0;
static Window trayMenu = 0;
static XFontStruct* font_info = NULL;
static char g_TimerText[32] = { 0 };
static char g_LockTitle[128] = { 0 };
static char g_LockSub[128] = { 0 };
static int drag_start_x = 0;
static int drag_start_y = 0;
static int is_dragging = 0;
static int timer_hidden = 0;

void toggle_console() {
    char* win_id_str = getenv("WINDOWID");
    if (win_id_str) {
        Window consoleWin = (Window)strtoul(win_id_str, NULL, 0);
        XWindowAttributes attrs;
        XGetWindowAttributes(dis, consoleWin, &attrs);
        if (attrs.map_state == IsViewable)
            XUnmapWindow(dis, consoleWin);
        else
            XMapWindow(dis, consoleWin);
    }
}

void toggle_timer() {
    timer_hidden = !timer_hidden;
    if (timer_hidden) {
        if (timerWin) XUnmapWindow(dis, timerWin);
    } else {
        if (timerWin) XMapWindow(dis, timerWin);
    }
}

void init_tray_icon() {
    char atom_name[32];
    snprintf(atom_name, sizeof(atom_name), "_NET_SYSTEM_TRAY_S%d", screen);
    Atom selection_atom = XInternAtom(dis, atom_name, False);
    Window tray_manager = XGetSelectionOwner(dis, selection_atom);
    
    if (tray_manager != None) {
        trayWin = XCreateSimpleWindow(dis, RootWindow(dis, screen), 0, 0, 24, 24, 0, BlackPixel(dis, screen), WhitePixel(dis, screen));
        
        XSelectInput(dis, trayWin, ButtonPressMask | ExposureMask);
        
        Atom XA_XEMBED_INFO = XInternAtom(dis, "_XEMBED_INFO", False);
        long info[2] = { 0, 1 }; // Version, Flags (1=Mapped)
        XChangeProperty(dis, trayWin, XA_XEMBED_INFO, XA_XEMBED_INFO, 32, PropModeReplace, (unsigned char*)info, 2);
        
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.xclient.type = ClientMessage;
        ev.xclient.window = tray_manager;
        ev.xclient.message_type = XInternAtom(dis, "_NET_SYSTEM_TRAY_OPCODE", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = CurrentTime;
        ev.xclient.data.l[1] = 0; // SYSTEM_TRAY_REQUEST_DOCK
        ev.xclient.data.l[2] = trayWin;
        
        XSendEvent(dis, tray_manager, False, NoEventMask, &ev);
    }
}

void draw_tray_icon() {
    if (!trayWin) return;
    XSetForeground(dis, DefaultGC(dis, screen), BlackPixel(dis, screen));
    XDrawString(dis, trayWin, DefaultGC(dis, screen), 4, 16, "AE", 2);
}

void init_window_system() {
     if (!dis) {
          dis = XOpenDisplay(NULL);
          if (dis) {
               screen = DefaultScreen(dis);
               // Try to load a decent font
               font_info = XLoadQueryFont(dis, "-*-helvetica-bold-r-normal--24-*-*-*-*-*-*-*");
               if (!font_info) font_info = XLoadQueryFont(dis, "fixed");
               init_tray_icon();
          }
     }
     printf("\033[?25l"); // Hide console cursor
}

void redraw_timer_pixmap() {
     if (!timerPixmap || !timerGC) return;
     XSetForeground(dis, timerGC, BlackPixel(dis, screen));
     XFillRectangle(dis, timerPixmap, timerGC, 0, 0, 160, 50);
     XSetForeground(dis, timerGC, 0x00FF00);
     XDrawRectangle(dis, timerPixmap, timerGC, 0, 0, 159, 49);
     if (font_info) {
          int len = strlen(g_TimerText);
          int tw = XTextWidth(font_info, g_TimerText, len);
          XDrawString(dis, timerPixmap, timerGC, (160 - tw) / 2, (50 + font_info->ascent) / 2 - 2, g_TimerText, len);
     }
}

void redraw_lock_pixmap(int width, int height) {
     if (!lockPixmap || !lockGC) return;
     XSetForeground(dis, lockGC, BlackPixel(dis, screen));
     XFillRectangle(dis, lockPixmap, lockGC, 0, 0, width, height);
     
     XSetForeground(dis, lockGC, WhitePixel(dis, screen));
     if (font_info) {
          int len = strlen(g_LockTitle);
          int tw = XTextWidth(font_info, g_LockTitle, len);
          XDrawString(dis, lockPixmap, lockGC, (width - tw) / 2, height / 2 - 20, g_LockTitle, len);

          len = strlen(g_LockSub);
          tw = XTextWidth(font_info, g_LockSub, len);
          XDrawString(dis, lockPixmap, lockGC, (width - tw) / 2, height / 2 + 20, g_LockSub, len);
     }
}

void update_window_events() {
     if (!dis) return;
     while (XPending(dis) > 0) {
          XEvent event;
          XNextEvent(dis, &event);

          // --- DRAGGING LOGIC ---
          if (event.type == ButtonPress && event.xbutton.window == timerWin) {
               if (event.xbutton.button == 1) { // Left Click
                    is_dragging = 1;
                    drag_start_x = event.xbutton.x_root;
                    drag_start_y = event.xbutton.y_root;
               }
          }
          else if (event.type == ButtonRelease) {
               if (event.xbutton.button == 1) is_dragging = 0;
          }
          else if (event.type == MotionNotify) {
               if (is_dragging && timerWin) {
                    int dx = event.xmotion.x_root - drag_start_x;
                    int dy = event.xmotion.y_root - drag_start_y;

                    XWindowAttributes attrs;
                    XGetWindowAttributes(dis, timerWin, &attrs);
                    XMoveWindow(dis, timerWin, attrs.x + dx, attrs.y + dy);

                    drag_start_x = event.xmotion.x_root;
                    drag_start_y = event.xmotion.y_root;
               }
          }

          // --- TRAY ICON LOGIC ---
          if (trayWin && event.xany.window == trayWin) {
               if (event.type == Expose) {
                    draw_tray_icon();
               } else if (event.type == ButtonPress) {
                    if (trayMenu) XDestroyWindow(dis, trayMenu);
                    trayMenu = XCreateSimpleWindow(dis, RootWindow(dis, screen), 
                         event.xbutton.x_root, event.xbutton.y_root - 50, 120, 50, 1, 
                         BlackPixel(dis, screen), WhitePixel(dis, screen));
                    XSetWindowAttributes attrs;
                    attrs.override_redirect = True;
                    XChangeWindowAttributes(dis, trayMenu, CWOverrideRedirect, &attrs);
                    XSelectInput(dis, trayMenu, ExposureMask | ButtonPressMask);
                    XMapWindow(dis, trayMenu);
                    XGrabPointer(dis, trayMenu, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
               }
          }
          
          if (trayMenu && event.xany.window == trayMenu) {
               if (event.type == Expose) {
                    XSetForeground(dis, DefaultGC(dis, screen), BlackPixel(dis, screen));
                    XDrawString(dis, trayMenu, DefaultGC(dis, screen), 10, 20, "Toggle Console", 14);
                    XDrawString(dis, trayMenu, DefaultGC(dis, screen), 10, 40, "Toggle Timer", 12);
               } else if (event.type == ButtonPress) {
                    if (event.xbutton.y < 25) toggle_console();
                    else toggle_timer();
                    
                    XUngrabPointer(dis, CurrentTime);
                    XDestroyWindow(dis, trayMenu);
                    trayMenu = 0;
               }
          }

          if (event.type == Expose && event.xexpose.count == 0) {
               if (timerWin && event.xany.window == timerWin) {
                    if (timerPixmap) {
                         // Copy from buffer to window (Flicker-free)
                         XCopyArea(dis, timerPixmap, timerWin, timerGC, 0, 0, 160, 50, 0, 0);
                    }
               }
               if (lockWin && event.xany.window == lockWin) {
                    if (lockPixmap) {
                         int width = DisplayWidth(dis, screen);
                         int height = DisplayHeight(dis, screen);
                         XCopyArea(dis, lockPixmap, lockWin, lockGC, 0, 0, width, height, 0, 0);
                    }
               }
          }
     }
}

void set_timer_window(int enable, const char* time_str) {
     if (!dis) return;
     if (enable) {
          // Auto-show if time is low (< 1 minute)
          int h, m, s;
          if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
               if (h == 0 && m == 0 && s < 60 && timer_hidden) {
                    timer_hidden = 0;
                    if (timerWin) XMapWindow(dis, timerWin);
               }
          }

          int changed = 0;
          if (strncmp(g_TimerText, time_str, 31) != 0) changed = 1;

          // Prevent blinking: Only redraw if text changed
          if (timerWin && !changed) return;

          strncpy(g_TimerText, time_str, 31);
          if (!timerWin) {
               int s_width = DisplayWidth(dis, screen);
               timerWin = XCreateSimpleWindow(dis, RootWindow(dis, screen), s_width - 180, 20, 160, 50, 0, BlackPixel(dis, screen), BlackPixel(dis, screen));
               XSetWindowAttributes attrs;
               attrs.override_redirect = True;
               XChangeWindowAttributes(dis, timerWin, CWOverrideRedirect, &attrs);
               XSelectInput(dis, timerWin, ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
               XMapWindow(dis, timerWin);
               timerGC = XCreateGC(dis, timerWin, 0, 0);
               if (font_info) XSetFont(dis, timerGC, font_info->fid);

               // Set Transparency (80%)
               unsigned long opacity = (unsigned long)(0xFFFFFFFF * 0.8);
               Atom opacity_atom = XInternAtom(dis, "_NET_WM_WINDOW_OPACITY", False);
               if (opacity_atom != None) {
                    XChangeProperty(dis, timerWin, opacity_atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity, 1);
               }
               
               // Create Double Buffer
               timerPixmap = XCreatePixmap(dis, timerWin, 160, 50, DefaultDepth(dis, screen));
               changed = 1; // Force initial draw
          }
          
          if (changed) {
               redraw_timer_pixmap();
               if (!timer_hidden) {
                    XCopyArea(dis, timerPixmap, timerWin, timerGC, 0, 0, 160, 50, 0, 0);
               }
          }
     }
     else if (timerWin) {
          if (timerPixmap) { XFreePixmap(dis, timerPixmap); timerPixmap = 0; }
          XDestroyWindow(dis, timerWin);
          timerWin = 0;
          timer_hidden = 0;
     }
     XFlush(dis);
}

void set_window_lockdown(int enable, const char* title, const char* subtitle) {
     if (!dis) return;
     if (enable) {
          // Prevent blinking: Check if text changed
          int changed = 0;
          if (strncmp(g_LockTitle, title ? title : "", 127) != 0) changed = 1;
          if (strncmp(g_LockSub, subtitle ? subtitle : "", 127) != 0) changed = 1;

          strncpy(g_LockTitle, title ? title : "", 127);
          strncpy(g_LockSub, subtitle ? subtitle : "", 127);

          if (!lockWin) {
               int width = DisplayWidth(dis, screen);
               int height = DisplayHeight(dis, screen);

               lockWin = XCreateSimpleWindow(dis, RootWindow(dis, screen), 0, 0, width, height, 0, BlackPixel(dis, screen), BlackPixel(dis, screen));

               XSetWindowAttributes attrs;
               attrs.override_redirect = True;
               XChangeWindowAttributes(dis, lockWin, CWOverrideRedirect, &attrs);
               XSelectInput(dis, lockWin, ExposureMask);
               XMapWindow(dis, lockWin);

               // Hide Cursor
               Cursor invisibleCursor;
               Pixmap bitmapNoData;
               XColor black;
               static char noData[] = { 0,0,0,0,0,0,0,0 };
               black.red = black.green = black.blue = 0;
               bitmapNoData = XCreateBitmapFromData(dis, lockWin, noData, 8, 8);
               invisibleCursor = XCreatePixmapCursor(dis, bitmapNoData, bitmapNoData, &black, &black, 0, 0);
               XDefineCursor(dis, lockWin, invisibleCursor);
               XFreeCursor(dis, invisibleCursor);
               XFreePixmap(dis, bitmapNoData);

               lockGC = XCreateGC(dis, lockWin, 0, 0);
               if (font_info) XSetFont(dis, lockGC, font_info->fid);
               
               // Create Double Buffer for Lock Screen
               lockPixmap = XCreatePixmap(dis, lockWin, width, height, DefaultDepth(dis, screen));
               changed = 1;
          }
          
          if (changed) {
               int width = DisplayWidth(dis, screen);
               int height = DisplayHeight(dis, screen);
               redraw_lock_pixmap(width, height);
               XCopyArea(dis, lockPixmap, lockWin, lockGC, 0, 0, width, height, 0, 0);
          }
          XRaiseWindow(dis, lockWin);
     }
     else if (lockWin) {
          if (lockPixmap) { XFreePixmap(dis, lockPixmap); lockPixmap = 0; }
          XDestroyWindow(dis, lockWin);
          lockWin = 0;
     }
     XFlush(dis);
}

// --- LINUX CONSOLE HELPERS ---
void console_set_color(int color_code) {
     switch (color_code) {
          case C_GREEN:  printf("\033[32m"); break;
          case C_RED:    printf("\033[31m"); break;
          case C_YELLOW: printf("\033[33m"); break;
          case C_BLUE:   printf("\033[34m"); break;
          case C_WHITE:  printf("\033[37m"); break;
          default:       printf("\033[0m"); break;
     }
}

void console_gotoxy(int x, int y) {
     printf("\033[%d;%dH", y + 1, x + 1);
}

void console_clear() {
     printf("\033[2J\033[H");
}

void get_console_size(int* width, int* height) {
     struct winsize w;
     if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0) {
          *width = 80; *height = 24;
     }
     else {
          *width = w.ws_col; *height = w.ws_row;
     }
}

long long get_current_time_ms() {
     struct timeval tv;
     gettimeofday(&tv, NULL);
     return (long long)tv.tv_sec * 1000 + (long long)tv.tv_usec / 1000;
}

void sleep_ms(int ms) {
     usleep(ms * 1000);
}