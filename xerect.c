#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define die(args...)                                                           \
  do {                                                                         \
    error(args);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

typedef struct SelectRegion SelectRegion;
struct SelectRegion {
   Window root;
   int x;          
   int y;          
   int X;
   int Y;          
   unsigned int w;
   unsigned int h;
   unsigned int b;
   unsigned int d;
};

typedef struct RetData RetData;
struct RetData {
  unsigned char *data;
  size_t size;
  // width
  unsigned int w;
  // hegit 
  unsigned int h; 
};

static void getScreen(Display *ptr_display, Window *ptr_root, const int xx,
                      const int yy, const int W, const int H,
                      // out 
                       unsigned char *data);
static void error(const char *errstr, ...);
static int select_region(Display *display, Window root, SelectRegion *region,
                         int xgrab);
RetData doGlabScreen() {
  Display *display;
  Window root;
  SelectRegion selected_region; 
  int xgrab = 0;

  display = XOpenDisplay(NULL);
  if (!display) {
    die("failed to open display %s\n", getenv("DISPLAY"));
  }

  root = DefaultRootWindow(display);

  // interactive selection
  if (select_region(display, root, &selected_region, xgrab) != EXIT_SUCCESS) {
    XCloseDisplay(display);
    die("failed to select a rectangular region\n");
  }
  // screen shot
  unsigned long data_size = selected_region.w * selected_region.h * 3;
  unsigned char *data = (unsigned char *)malloc(data_size);
  if (!data) {
    XCloseDisplay(display);
    die("failed to malloc");
  }
  getScreen(display, &root, selected_region.x, selected_region.y,
            selected_region.w, selected_region.h, data);
  XDestroyWindow(display, root);
  XCloseDisplay(display);
  RetData r = {.data = data,
               .size = data_size,
               .w = selected_region.w,
               .h = selected_region.h};
  return r;
}
void getScreen(Display *ptr_display, Window *ptr_root, const int xx,
               const int yy, const int W, const int H,
               // out
               unsigned char *data) {

  Display *display = ptr_display;
  Window root = *ptr_root;

  XImage *image = XGetImage(display, root, xx, yy, W, H, AllPlanes, ZPixmap);

  unsigned long red_mask = image->red_mask;
  unsigned long green_mask = image->green_mask;
  unsigned long blue_mask = image->blue_mask;
  int x, y;
  int ii = 0;
  for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
      unsigned long pixel = XGetPixel(image, x, y);
      unsigned char blue = (pixel & blue_mask);
      unsigned char green = (pixel & green_mask) >> 8;
      unsigned char red = (pixel & red_mask) >> 16;

      data[ii + 2] = blue;
      data[ii + 1] = green;
      data[ii + 0] = red;
      ii += 3;
    }
  }
  XDestroyImage(image);
  return;
}

static void error(const char *errstr, ...) {
  va_list ap;

  fprintf(stderr, "xserect: ");
  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
}

static int select_region(Display *display, Window root, SelectRegion *region,
                         int xgrab) {
  XEvent ev;

  GC sel_gc;
  XGCValues sel_gv;

  int status, done = 0, btn_pressed = 0;
  int x = 0, y = 0;
  unsigned int width = 0, height = 0;
  int start_x = 0, start_y = 0;

  Cursor cursor;
  cursor = XCreateFontCursor(display, XC_tcross);

  // mask of events
  status =
      XGrabPointer(display, root, True,
                   PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                   GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);

  if (status != GrabSuccess) {
    error("failed to grab pointer\n");
    return EXIT_FAILURE;
  }

  sel_gv.function = GXinvert;
  sel_gv.subwindow_mode = IncludeInferiors;
  sel_gv.line_width = 1;
  sel_gc = XCreateGC(display, root, GCFunction | GCSubwindowMode | GCLineWidth,
                     &sel_gv);

  for (;;) {
    XNextEvent(display, &ev);
    switch (ev.type) {
    case ButtonPress:
      btn_pressed = 1;
      x = start_x = ev.xbutton.x_root;
      y = start_y = ev.xbutton.y_root;
      width = height = 0;
      if (xgrab) {
        XGrabServer(display);
      }
      break;
    case MotionNotify:
      // draw lines only if btn_pressed == 1
      if (btn_pressed) {
        // redraw
        XDrawRectangle(display, root, sel_gc, x, y, width, height);

        x = ev.xbutton.x_root;
        y = ev.xbutton.y_root;

        if (x > start_x) {
          width = x - start_x;
          x = start_x;
        } else {
          width = start_x - x;
        }
        if (y > start_y) {
          height = y - start_y;
          y = start_y;
        } else {
          height = start_y - y;
        }

        // draw
        XDrawRectangle(display, root, sel_gc, x, y, width, height);
        XFlush(display);
      }
      break;
    case ButtonRelease:
      done = 1;
      break;
    default:
      break;
    }
    if (done)
      break;
  }
  // clear lefted rect
  XDrawRectangle(display, root, sel_gc, x, y, width, height);
  XFlush(display);

  XUngrabServer(display);
  XUngrabPointer(display, CurrentTime);
  XFreeCursor(display, cursor);
  XFreeGC(display, sel_gc);
  XSync(display, 1);

  // root region
  SelectRegion rr; 
  SelectRegion selected_region;

  if (False == XGetGeometry(display, root, &rr.root, &rr.x, &rr.y, &rr.w, &rr.h,
                            &rr.b, &rr.d)) {
    error("failed to get root window geometry\n");
    return EXIT_FAILURE;
  }
  selected_region.x = x;
  selected_region.y = y;
  selected_region.w = width;
  selected_region.h = height;
  // calculate right/bottom offset
  selected_region.X = rr.w - selected_region.x - selected_region.w;
  selected_region.Y = rr.h - selected_region.y - selected_region.h;
  selected_region.b = rr.b;
  selected_region.d = rr.d;
  *region = selected_region;
  return EXIT_SUCCESS;
}

