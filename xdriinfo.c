/*
 * Client application for querying drivers' configuration information
 * Copyright (C) 2003 Felix Kuehling
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * FELIX KUEHLING, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <xf86drm.h>

typedef const char * glXGetScreenDriver_t (Display *dpy, int scrNum);
typedef const char * glXGetDriverConfig_t (const char *driverName);

static glXGetScreenDriver_t *GetScreenDriver;
static glXGetDriverConfig_t *GetDriverConfig;

enum INFO_FUNC {
    LIST, NSCREENS, DRIVER, OPTIONS
};

const char *name = NULL;

/* inspried by libva */
struct driver_name_map {
    const char *key;
    int         key_len;
    const char *name;
};

static const struct driver_name_map g_driver_name_map[] = {
    { "i915",       4, "i965"   }, // Intel OTC GenX driver
    { "radeon",     6, "r600"     }, // Mesa Gallium driver
    { "amdgpu",     6, "radeonsi" }, // Mesa Gallium driver
    { NULL, }
};

static char * get_drm_device(int fd_id) {
   int drm_fd;
   drmVersionPtr drm_version;
   const char drm_path[] = "/dev/dri/card";
   const char *drm_devname;
   char *path;
   size_t size;
   const struct driver_name_map *m;

   size = sizeof drm_path + sizeof fd_id;
   path = malloc(size);
   if (!path)
       return NULL;
   snprintf(path, size, "%s%d", drm_path, fd_id);
   if ((drm_fd = open (path, O_RDWR|O_CLOEXEC)) < 0)
       return NULL;
   drm_version = drmGetVersion(drm_fd);
   if (!drm_version) {
       fprintf(stderr, "Unable to get DRM version, weird.");
       close(drm_fd);
       drmFreeVersion (drm_version);
       return NULL;
   }
   close(drm_fd);
   for (m = g_driver_name_map; m->key != NULL; m++) {
       if (drm_version->name_len >= m->key_len &&
           strncmp(drm_version->name, m->key, m->key_len) == 0)
           break;
   }
   drmFreeVersion(drm_version);

   if (!m->name)
       return NULL;

   drm_devname = strdup(m->name);
   if (!drm_devname)
       return NULL;

   return (char *) drm_devname;
}

void printUsage (void);

void printUsage (void) {
    fprintf (stderr,
"Usage: xdriinfo [-display <dpy>] [-version] [command]\n"
"Commands:\n"
"  nscreens               print the number of screens on display\n"
"  driver screen          print the DRI driver name of screen\n"
"  options screen|driver  print configuration information about screen or driver\n"
"If no command is given then the DRI drivers for all screens are listed.\n");
}

int main (int argc, char *argv[]) {
    Display *dpy;
    int nScreens, screenNum, i;
    enum INFO_FUNC func = LIST;
    char *funcArg = NULL;
    char *dpyName = NULL;

    GetScreenDriver = (glXGetScreenDriver_t *)glXGetProcAddressARB ((const GLubyte *)"glXGetScreenDriver");
    GetDriverConfig = (glXGetDriverConfig_t *)glXGetProcAddressARB ((const GLubyte *)"glXGetDriverConfig");
    if (!GetScreenDriver || !GetDriverConfig) {
	fprintf (stderr, "libGL is too old.\n");
	return 1;
    }

  /* parse the command line */
    for (i = 1; i < argc; ++i) {
	char **argPtr = NULL;
	if (!strcmp (argv[i], "-display"))
	    argPtr = &dpyName;
	else if (!strcmp (argv[i], "nscreens"))
	    func = NSCREENS;
	else if (!strcmp (argv[i], "driver")) {
	    func = DRIVER;
	    argPtr = &funcArg;
	} else if (!strcmp (argv[i], "options")) {
	    func = OPTIONS;
	    argPtr = &funcArg;
	} else if (!strcmp (argv[i], "-version")) {
	    puts(PACKAGE_STRING);
	    return 0;
	} else {
	    fprintf (stderr, "%s: unrecognized argument '%s'\n",
		     argv[0], argv[i]);
	    printUsage ();
	    return 1;
	}
	if (argPtr) {
	    if (++i == argc) {
		fprintf (stderr, "%s: '%s' requires an argument\n",
			 argv[0], argv[i-1]);
		printUsage ();
		return 1;
	    }
	    *argPtr = argv[i];
	}
    }

  /* parse screen number argument */
    if (func == DRIVER || func == OPTIONS) {
	if (sscanf (funcArg, "%i", &screenNum) != 1)
	    screenNum = -1;
	else if (screenNum < 0) {
	    fprintf (stderr, "Negative screen number \"%s\".\n", funcArg);
	    return 1;
	}
    }

  /* driver command needs a valid screen number */
    if (func == DRIVER && screenNum == -1) {
	fprintf (stderr, "Invalid screen number \"%s\".\n", funcArg);
	return 1;
    }

  /* open display and count the number of screens */
    if (!(dpy = XOpenDisplay (dpyName))) {
	fprintf (stderr, "Error: Couldn't open display\n");
	return 1;
    }
    nScreens = ScreenCount (dpy);

  /* final check on the screen number argument (if any)*/
    if ((func == DRIVER || func == OPTIONS) && screenNum >= nScreens) {
	fprintf (stderr, "Screen number \"%d\" out of range.\n", screenNum);
	return 1;
    }

   /* Call glXGetClientString to load vendor libs on glvnd enabled systems */
    glXGetClientString (dpy, GLX_EXTENSIONS);

    switch (func) {
      case NSCREENS:
	printf ("%d", nScreens);
	if (isatty (STDOUT_FILENO))
	    printf ("\n");
	break;
      case DRIVER: {
	  if (!(name = (*GetScreenDriver) (dpy, screenNum)))
              name = get_drm_device (screenNum);
	  if (!name) {
	      fprintf (stderr, "Screen \"%d\" is not direct rendering capable.\n",
		       screenNum);
	      return 1;
	  }
	  printf ("%s", name);
	  if (isatty (STDOUT_FILENO))
	      printf ("\n");
	  break;
      }
      case OPTIONS: {
	  const char *options;

	  if (screenNum == -1) {
	      name = funcArg;
	  } else {
	      if (!(name = (*GetScreenDriver) (dpy, screenNum)))
                  name = get_drm_device (screenNum);
	  }
	  if (!name) {
	      fprintf (stderr, "Screen \"%d\" is not direct rendering capable.\n",
		       screenNum);
	      return 1;
	  }
	  options = (*GetDriverConfig) (name);
	  if (!options) {
	      fprintf (stderr,
		       "Driver \"%s\" is not installed or does not support configuration.\n",
		       name);
	      return 1;
	  }
	  printf ("%s", options);
	  if (isatty (STDOUT_FILENO))
	      printf ("\n");
	  break;
      }
      case LIST:
	for (i = 0; i < nScreens; ++i) {
	    if (!(name = (*GetScreenDriver) (dpy, i)))
		name = get_drm_device (i);
	    if (name)
		printf ("Screen %d: %s\n", i, name);
	    else
		printf ("Screen %d: not direct rendering capable.\n", i);
	}
    }

    return 0;
}
