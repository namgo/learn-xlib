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

// 60FPS is 16.3ms/frame

void convert(XImage* image, int width, int height, uint8_t* buffer) {
  uint8_t *bp = buffer;
  for (int ly = 0; ly < height; ly++) {
    for (int lx = 0; lx < width; lx++) {
      uint32_t px = XGetPixel(image, lx, ly);
      (*bp++) = px>>16;
      (*bp++) = px>>8;
      (*bp++) = px;
    }
  }
}

 
void screenshot(Display *disp, Window w, XWindowAttributes attribs, uint8_t *buffer) {
  XImage *image;
  XMapWindow(disp, w);
  image = XGetImage(disp, w, 0, 0, attribs.width, attribs.height, AllPlanes, ZPixmap);
  convert(image, attribs.width, attribs.height, buffer);
  XDestroyImage(image);
}

struct args {
  Window w;
  Display *display;
  XWindowAttributes attribs;
  uint8_t *buffer;
};

static void *start(void* arg) {

  struct args *a = (struct args*)arg;

  struct timespec ts;
  ts.tv_sec = 16 / 1000;
  ts.tv_nsec = (16 % 1000) * 1000000;

  while (1) {
    screenshot(a->display, a->w, a->attribs, a->buffer);
    nanosleep(&ts, &ts);
  }  
}
int roundUp(int numToRound, int multiple)
{
    if (multiple == 0)
        return numToRound;

    int remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}
int main(int argc, char** argv) {
  Display *display = XOpenDisplay(NULL);
  Window _root = DefaultRootWindow(display);
  Window root;
  Window parent;
  Window* children;
  unsigned int nchildren;
  int c = 0;
  
  XQueryTree(display, _root, &root, &parent, &children, &nchildren);
  Window w;
  XWindowAttributes attribs;
  for (int i = 0; i < nchildren; i++) {
    
    
    XGetWindowAttributes(display, children[i], &attribs);

    if (attribs.class != InputOnly && attribs.width > 40) {
      w = children[i];
      c = 1;
    }
  }
  
  struct args a;

  uint8_t* buffer = (uint8_t*)malloc(attribs.width * attribs.height * 3);
  a.attribs = attribs;
  a.buffer = buffer;
  a.display = display;
  a.w = w;
  /*
  XImage *image;
  XShmSegmentInfo shminfo;
  image = XShmCreateImage(display,
			  DefaultVisual(display,0), // Use a correct visual. Omitted for brevity     
			  8,   // Determine correct depth from the visual. Omitted for brevity
			  ZPixmap, NULL, &shminfo, attribs.width, attribs.height);

   shminfo.shmid = shmget(IPC_PRIVATE,
			  image->bytes_per_line * image->height,
			  IPC_CREAT|0777);
   shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
   shminfo.readOnly = False;
   XShmAttach(display, &shminfo);
   XShmGetImage(display,
		RootWindow(display, 0),
		image,
		100,
		100,
		AllPlanes);
  */
  pthread_t ti;
  pthread_create(&ti, NULL, start, &a);
  rfbScreenInfoPtr server = rfbGetScreen(&argc, argv, attribs.width, attribs.height, 8, 3, 4);
  rfbNewFramebuffer(server, buffer, attribs.width, attribs.height, 8, 4, 3);
  rfbInitServer(server);

  pthread_t t;
  
  while (rfbIsActive(server)) {
    rfbMarkRectAsModified(server, 0, 0, attribs.width, attribs.height);
    rfbProcessEvents(server, server->deferUpdateTime * 60);
  }

  pthread_join(ti, NULL);
}
