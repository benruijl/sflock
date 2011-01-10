/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fontconfig/fontconfig.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>
#include <X11/Xft/Xft.h>

#if HAVE_BSD_AUTH
#include <login_cap.h>
#include <bsd_auth.h>
#endif

static void
die(const char *errstr, ...) {
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

#ifndef HAVE_BSD_AUTH
static const char *
get_password() { /* only run as root */
    const char *rval;
    struct passwd *pw;

    if(geteuid() != 0)
        die("sflock: cannot retrieve password entry (make sure to suid sflock)\n");
    pw = getpwuid(getuid());
    endpwent();
    rval =  pw->pw_passwd;

#if HAVE_SHADOW_H
    {
        struct spwd *sp;
        sp = getspnam(getenv("USER"));
        endspent();
        rval = sp->sp_pwdp;
    }
#endif

    /* drop privileges */
    if(setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0)
        die("sflock: cannot drop privileges\n");
    return rval;
}
#endif

int
main(int argc, char **argv) {
    char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    char buf[32], passwd[256], passdisp[256];
    int num, screen, width, height, update, sleepmode;

#ifndef HAVE_BSD_AUTH
    const char *pws;
#endif
    unsigned int len;
    Bool running = True;
    Cursor invisible;
    Display *dpy;
    KeySym ksym;
    Pixmap pmap;
    Window root, w;
    XColor black, red, dummy;
    XEvent ev;
    XSetWindowAttributes wa;
    XftFont *font;
    XftColor xftcolor;
    XftDraw *xftdraw;
    XGlyphInfo extents;
    GC gc; 
    XGCValues values;

    // defaults
    char passchar = '*';
    float baroffset= 60;
    char* fontname = "-*-verdana-bold-r-*-*-*-420-100-100-*-*-iso8859-1";

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-c")) {
            if (i + 1 < argc) 
                passchar = argv[i + 1][0];
            else
                die("error: no password character given.\n");
        } else
            if (!strcmp(argv[i], "-f")) {
                if (i + 1 < argc) 
                    fontname = argv[i + 1];
                else
                    die("error: font not specified.\n");
            }
        if (!strcmp(argv[i], "-b")) {
            if (i + 1 < argc) 
                baroffset = strtof(argv[i + 1], 0);
            else
                die("error: bar offset not specified.\n");
        }
        else
            if (!strcmp(argv[i], "-v")) 
                die("sflock-"VERSION", © 2010 Ben Ruijl\n");
            else 
                if (!strcmp(argv[i], "?")) 
                    die("usage: sflock [-v] [-c passchar] [-f fontname] [-b bar offset]\n");
    }

    // fill with password character
    for (int i = 0; i < sizeof passdisp; i++) {
        passdisp[i] = passchar;
    }

#ifndef HAVE_BSD_AUTH
    pws = get_password();
#endif

    if(!(dpy = XOpenDisplay(0)))
        die("sflock: cannot open dpy\n");

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    width = DisplayWidth(dpy, screen);
    height = DisplayHeight(dpy, screen);

    wa.override_redirect = 1;
    wa.background_pixel = XBlackPixel(dpy, screen);
    w = XCreateWindow(dpy, root, 0, 0, width, height,
            0, DefaultDepth(dpy, screen), CopyFromParent,
            DefaultVisual(dpy, screen), CWOverrideRedirect | CWBackPixel, &wa);

    XAllocNamedColor(dpy, DefaultColormap(dpy, screen), "orange red", &red, &dummy);
    XAllocNamedColor(dpy, DefaultColormap(dpy, screen), "black", &black, &dummy);
    pmap = XCreateBitmapFromData(dpy, w, curs, 8, 8);
    invisible = XCreatePixmapCursor(dpy, pmap, pmap, &black, &black, 0, 0);
    XDefineCursor(dpy, w, invisible);
    XMapRaised(dpy, w);

    xftdraw = XftDrawCreate(dpy, w, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
    font = XftFontOpenXlfd(dpy, screen, fontname);
    XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), "white", &xftcolor);

    if (font == 0) {
        die("error: could not find font. Try using a full description.\n");
    }

    gc = XCreateGC(dpy, w, (unsigned long)0, &values);
    XSetForeground(dpy, gc, XWhitePixel(dpy, screen));

    for(len = 1000; len; len--) {
        if(XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime) == GrabSuccess)
            break;
        usleep(1000);
    }
    if((running = running && (len > 0))) {
        for(len = 1000; len; len--) {
            if(XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
                    == GrabSuccess)
                break;
            usleep(1000);
        }
        running = (len > 0);
    }
    len = 0;
    XSync(dpy, False);
    update = True;
    sleepmode = False;

    /* main event loop */
    while(running && !XNextEvent(dpy, &ev)) {
        if (sleepmode) {
            DPMSEnable(dpy);
            DPMSForceLevel(dpy, DPMSModeOff);
            XFlush(dpy);

        }

        if (update) {
            XClearWindow(dpy, w);
            XDrawLine(dpy, w, gc, width * 3 / 8 , (height + baroffset) / 2, width * 5 / 8, (height + baroffset) / 2);
            XftTextExtentsUtf8(dpy, font, (XftChar8 *)passdisp, len, &extents);
            XftDrawStringUtf8(xftdraw, &xftcolor, font, (width - extents.width) / 2, (height+42) / 2, (XftChar8 *)passdisp, len);
            update = False;
        }

        if (ev.type == MotionNotify) {
            sleepmode = False;
        }

        if(ev.type == KeyPress) {
            sleepmode = False;

            buf[0] = 0;
            num = XLookupString(&ev.xkey, buf, sizeof buf, &ksym, 0);
            if(IsKeypadKey(ksym)) {
                if(ksym == XK_KP_Enter)
                    ksym = XK_Return;
                else if(ksym >= XK_KP_0 && ksym <= XK_KP_9)
                    ksym = (ksym - XK_KP_0) + XK_0;
            }
            if(IsFunctionKey(ksym) || IsKeypadKey(ksym)
                    || IsMiscFunctionKey(ksym) || IsPFKey(ksym)
                    || IsPrivateKeypadKey(ksym))
                continue;

            switch(ksym) {
                case XK_Return:
                    passwd[len] = 0;
#ifdef HAVE_BSD_AUTH
                    running = !auth_userokay(getlogin(), NULL, "auth-xlock", passwd);
#else
                    running = strcmp(crypt(passwd, pws), pws);
#endif
                    if (running != 0)
                        // change bakground on wrong password
                        XSetWindowBackground(dpy, w, red.pixel);
                    len = 0;
                    break;
                case XK_Escape:
                    len = 0;

                    if (DPMSCapable(dpy)) {
                        sleepmode = True;
                    }

                    break;
                case XK_BackSpace:
                    if(len)
                        --len;
                    break;
                default:
                    if(num && !iscntrl((int) buf[0]) && (len + num < sizeof passwd)) { 
                        memcpy(passwd + len, buf, num);
                        len += num;
                    }

                    break;
            }

            update = True; // show changes
        }
    }

    XUngrabPointer(dpy, CurrentTime);
    XFreePixmap(dpy, pmap);
    XftFontClose(dpy, font);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &xftcolor);
    XftDrawDestroy(xftdraw);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, w);
    XCloseDisplay(dpy);
    return 0;
}
