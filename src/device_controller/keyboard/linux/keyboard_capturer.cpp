#include "keyboard_capturer.h"

#include "keyboard_converter.h"
#include "rd_log.h"

namespace crossdesk {

static OnKeyAction g_on_key_action = nullptr;
static void* g_user_ptr = nullptr;

static int KeyboardEventHandler(Display* display, XEvent* event) {
  if (event->xkey.type == KeyPress || event->xkey.type == KeyRelease) {
    KeySym keySym = XKeycodeToKeysym(display, event->xkey.keycode, 0);
    int key_code = XKeysymToKeycode(display, keySym);
    bool is_key_down = (event->xkey.type == KeyPress);

    if (g_on_key_action) {
      g_on_key_action(key_code, is_key_down, g_user_ptr);
    }
  }
  return 0;
}

KeyboardCapturer::KeyboardCapturer() : display_(nullptr), running_(true) {
  display_ = XOpenDisplay(nullptr);
  if (!display_) {
    LOG_ERROR("Failed to open X display.");
  }
}

KeyboardCapturer::~KeyboardCapturer() {
  if (display_) {
    XCloseDisplay(display_);
  }
}

int KeyboardCapturer::Hook(OnKeyAction on_key_action, void* user_ptr) {
  g_on_key_action = on_key_action;
  g_user_ptr = user_ptr;

  XSelectInput(display_, DefaultRootWindow(display_),
               KeyPressMask | KeyReleaseMask);

  while (running_) {
    XEvent event;
    XNextEvent(display_, &event);
    KeyboardEventHandler(display_, &event);
  }

  return 0;
}

int KeyboardCapturer::Unhook() {
  g_on_key_action = nullptr;
  g_user_ptr = nullptr;

  running_ = false;

  if (display_) {
    XSelectInput(display_, DefaultRootWindow(display_), 0);
    XFlush(display_);
  }

  return 0;
}

int KeyboardCapturer::SendKeyboardCommand(int key_code, bool is_down) {
  if (!display_) {
    LOG_ERROR("Display not initialized.");
    return -1;
  }

  if (vkCodeToX11KeySym.find(key_code) != vkCodeToX11KeySym.end()) {
    int x11_key_code = vkCodeToX11KeySym[key_code];
    KeyCode keycode = XKeysymToKeycode(display_, x11_key_code);
    XTestFakeKeyEvent(display_, keycode, is_down, CurrentTime);
    XFlush(display_);
  }
  return 0;
}
}  // namespace crossdesk