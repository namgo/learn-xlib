#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <X11/Xatom.h>

void save(XImage* image, int width, int height, int cnt) {
  printf("%d %d\n", width, height);
  uint8_t *buffer;
  uint8_t * bp = buffer = malloc( width * height * 3 );
  char* name = (char*)malloc(10);
  sprintf(name, "f-%i.bmp", cnt);  
  for (int ly = 0; ly < height; ly++) {
    for (int lx = 0; lx < width; lx++) {
      uint32_t px = XGetPixel(image, lx, ly);
      (*bp++) = px>>16;
      (*bp++) = px>>8;
      (*bp++) = px;
    }
  }

  FILE* f = fopen(name, "wb");
  fprintf( f, "P6\n%d %d\n255\n#comment\n", width, height );
  fwrite(buffer, 1, width*height*3, f);
  fclose(f);
  free(buffer);
  free(name);
}

void screenshot(Display *disp, Window w, int cnt) {
  XWindowAttributes attribs;
  XImage *image;
  XGetWindowAttributes(disp, w, &attribs);
  if(attribs.class != InputOnly) {
    XMapWindow(disp, w);
    image = XGetImage(disp, w, 0, 0, attribs.width, attribs.height, AllPlanes, ZPixmap);
    save(image, attribs.width, attribs.height, cnt);
    XDestroyImage(image);
  }
} 

int main() {
  Display *disp = XOpenDisplay(NULL);
  Window wroot = DefaultRootWindow(disp);

  Window root;
  Window parent;
  Window* children;
  unsigned int nchildren;

  XQueryTree(disp, wroot, &root, &parent, &children, &nchildren);
  printf("%d\n", nchildren);

  for (int i = 0; i < nchildren; i++) {
    screenshot(disp, children[i], i);
  }
  XCloseDisplay(disp);

}
