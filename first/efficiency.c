#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>

#include "rfb/rfb.h"

struct Selection {
  Display *display;
  Window choice;
  XWindowAttributes attr;
  short found;
};

struct FrameBuffer {
  struct Selection sel;
  uint8_t* data;
  XShmSegmentInfo shminfo;
  XImage *image;
};

int has_name(Display *display, Window w) {
  int r = 0;
  char* name = NULL;
  XFetchName(display, w, &name);
  printf("%s\n", name);
  if (strlen(name) > 1) {
    r = 1;
  }
  return r;
}

struct Selection choose_window_child(Display* display, Window root, int minwidth) {
  struct Selection selection;
  
  Window _root; 
  Window parent;
  Window *children;
  unsigned int n;
  int i, status;

  XWindowAttributes attr;

  selection.display = display;
  selection.found = 0;

  status = XQueryTree(display, root, &_root, &parent, &children, &n);
  
  if (status == 0) {
    return selection;
  }

  for (i = 0; i < n; i++) {
    XGetWindowAttributes(display, children[i], &attr);

    if (attr.class != InputOnly && attr.width > minwidth) {
      if (has_name(display, children[i])) {
	selection.attr = attr;
	selection.choice = children[i];
	selection.found = 1;
	break;
      }
    }
  }
  
  return selection;
}

void mk_screenshot(struct FrameBuffer *fb) {
  XImage *image;
  int ly, lx;
  uint32_t px;
  uint8_t *bp;
  XMapWindow(fb->sel.display, fb->sel.choice);
  image = XGetImage(fb->sel.display, fb->sel.choice, 0, 0,
		    fb->sel.attr.width, fb->sel.attr.height,
		    AllPlanes, ZPixmap);
  
  bp = fb->data;
  for (ly = 0; ly < fb->sel.attr.height; ly++) {
    for (lx = 0; lx < fb->sel.attr.width; lx++) {
      px = XGetPixel(image, lx, ly);
      (*bp++) = px>>16;
      (*bp++) = px>>8;
      (*bp++) = px;
    }
  }

  XDestroyImage(image);
}

static void* screenshot(void* arg) {
  struct FrameBuffer *fb = arg;
  struct timespec ts;
  ts.tv_sec = 16 / 1000;
  ts.tv_nsec = (16 % 1000) * 1000000;

  while (1) {
    mk_screenshot(fb);
    nanosleep(&ts, &ts);
  }
  return 0;
}

int main(int argc, char **argv) {
  struct FrameBuffer fb;

  pthread_t ti;

  rfbScreenInfoPtr server;
  
  Display *display = XOpenDisplay(NULL);
  Window root = DefaultRootWindow(display);

  fb.sel = choose_window_child(display, root, 80);
  if (!fb.sel.found) {
    printf("couldn't find window\n");
  }
  fb.data = (uint8_t*)malloc(fb.sel.attr.width * fb.sel.attr.height * 3);

  pthread_create(&ti, NULL, screenshot, &fb);

  server = rfbGetScreen(&argc, argv, fb.sel.attr.width, fb.sel.attr.height, 8, 4, 3);
  rfbNewFramebuffer(server, (char*)fb.data, fb.sel.attr.width, fb.sel.attr.height, 8, 4, 3);
  rfbInitServer(server);
  
  while (rfbIsActive(server)) {
    rfbMarkRectAsModified(server, 0, 0, fb.sel.attr.width, fb.sel.attr.height);
    rfbProcessEvents(server, server->deferUpdateTime * 600);
  }

  pthread_join(ti, NULL);
  
}
