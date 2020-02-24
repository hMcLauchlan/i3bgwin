#define _DEFAULT_SOURCE

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  int ignored;
  if (!XShapeQueryExtension(display, &ignored, &ignored)) {
    fprintf(stderr, "No SHAPE extension\n");
    exit(1);
  }

  int screen = DefaultScreen(display);

  // parse for screen args
  int dims[4] = {0, 0,DisplayWidth(display, screen), DisplayHeight(display, screen)};
  fprintf(stderr,"ree %d", dims[0]);
  fprintf(stderr,"ree %d", dims[1]);
  fprintf(stderr,"ree %d", dims[2]);
  fprintf(stderr,"ree %d", dims[3]);

  int arg_start = 0;
  for (;;++arg_start) {
      if (argv[arg_start + 1] && strcmp(argv[arg_start + 1], "--") == 0) {
          break;
      }
      dims[arg_start] = atoi(argv[arg_start + 1]);
  }
  ++arg_start;
  fprintf(stderr, "args_start %d", arg_start);

  XVisualInfo vinfo;
  XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo);

  XSetWindowAttributes attrs = {};
  attrs.event_mask =
      SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask | ExposureMask;
  attrs.do_not_propagate_mask = 0;
  attrs.colormap = XCreateColormap(display, RootWindow(display, screen), vinfo.visual, AllocNone);
  attrs.border_pixel = 0;
  attrs.background_pixel = 0;
  attrs.override_redirect = True;

  fprintf(stderr, "%d", DisplayHeight(display, screen));
  Window window = XCreateWindow(
      display, RootWindow(display, screen), dims[0], dims[1], dims[2], dims[3],  0, vinfo.depth, InputOutput, vinfo.visual,
      CWEventMask | CWColormap | CWBorderPixel | NoEventMask | CWBackPixel | CWOverrideRedirect,
      &attrs);
      /*
  Window window = XCreateWindow(
      display, RootWindow(display, screen), 0, 0, 4000, 2560,  0, vinfo.depth, InputOutput, vinfo.visual,
      CWEventMask | CWColormap | CWBorderPixel | NoEventMask | CWBackPixel | CWOverrideRedirect,
      &attrs);
  */

  Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);

  Atom netWmWindowType = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  Atom netWmWindowTypeDesktop = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);

  //Atom netWmWindowTypeFullScreenMonitors = XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", False);
  XChangeProperty(display, window, netWmWindowType, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&netWmWindowTypeDesktop, 1);

  XLowerWindow(display, window);

  Region region = XCreateRegion();
  XShapeCombineRegion(display, window, ShapeInput, 0, 0, region, ShapeSet);
  XDestroyRegion(region);

  XMapWindow(display, window);

  pid_t pid = fork();
  if (pid == -1) {
    perror("Failed to fork");
    exit(1);
  } else if (pid == 0) {
    char window_id[64];
    sprintf(window_id, "%lu", window);

    char **child_argv = calloc(argc - arg_start, sizeof(char *));
    for (int i = arg_start; i < argc; ++i) {
      if (argv[i + 1] && strcmp(argv[i + 1], "{windowid}") == 0) {
        child_argv[i - arg_start] = window_id;
      } else {
        child_argv[i - arg_start] = argv[i + 1];
        fprintf(stderr, "ree %d", i - arg_start);
      }
      fprintf(stderr,"args %s\n", child_argv[i - arg_start]);
    }

    execvp(child_argv[0], child_argv);

    perror("Can't execute child process!");
    _exit(1);
  }

  Display *child_display = NULL;
  Window child_window = 0;

  while (1) {
    XEvent event;
    XNextEvent(display, &event);

    if (event.type == MapRequest) {
      XMapWindow(event.xmaprequest.display, event.xmaprequest.window);
      child_display = event.xmaprequest.display;
      child_window = event.xmaprequest.window;
    } else if (event.type == ConfigureNotify || event.type == MapRequest) {
      if (child_window) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, window, &attrs);
        XMoveResizeWindow(child_display, child_window, 0, 0, attrs.width, attrs.height);
      }
    } else if (event.type == Expose) {
      XClearWindow(display, window);
    } else if (event.type == DestroyNotify) {
      break;
    } else if (event.type == ClientMessage && event.xclient.data.l[0] == (long)wmDeleteMessage) {
      break;
    }
  }

  kill(pid, SIGTERM);
  wait(0);

  XCloseDisplay(display);
  return 0;
}
