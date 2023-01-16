#import <Cocoa/Cocoa.h>
#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif /// of __APPLE__
#include <FL/x.H>
#include <FL/Fl_Window.H>

void setTransparency(Fl_Window *w, double alpha) {
    [fl_xid(w) setAlphaValue:alpha];
}
