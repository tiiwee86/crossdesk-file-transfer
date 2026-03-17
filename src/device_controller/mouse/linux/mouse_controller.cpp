#include "mouse_controller.h"

#include <X11/extensions/XTest.h>

#include "rd_log.h"

namespace crossdesk {

MouseController::MouseController() {}

MouseController::~MouseController() { Destroy(); }

int MouseController::Init(std::vector<DisplayInfo> display_info_list) {
  display_info_list_ = display_info_list;
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    LOG_ERROR("Cannot connect to X server");
    return -1;
  }

  root_ = DefaultRootWindow(display_);

  int event_base, error_base, major_version, minor_version;
  if (!XTestQueryExtension(display_, &event_base, &error_base, &major_version,
                           &minor_version)) {
    LOG_ERROR("XTest extension not available");
    XCloseDisplay(display_);
    return -2;
  }

  return 0;
}

int MouseController::Destroy() {
  if (display_) {
    XCloseDisplay(display_);
    display_ = nullptr;
  }
  return 0;
}

int MouseController::SendMouseCommand(RemoteAction remote_action,
                                      int display_index) {
  switch (remote_action.type) {
    case mouse:
      switch (remote_action.m.flag) {
        case MouseFlag::move:
          SetMousePosition(
              static_cast<int>(remote_action.m.x *
                                   display_info_list_[display_index].width +
                               display_info_list_[display_index].left),
              static_cast<int>(remote_action.m.y *
                                   display_info_list_[display_index].height +
                               display_info_list_[display_index].top));
          break;
        case MouseFlag::left_down:
          XTestFakeButtonEvent(display_, 1, True, CurrentTime);
          XFlush(display_);
          break;
        case MouseFlag::left_up:
          XTestFakeButtonEvent(display_, 1, False, CurrentTime);
          XFlush(display_);
          break;
        case MouseFlag::right_down:
          XTestFakeButtonEvent(display_, 3, True, CurrentTime);
          XFlush(display_);
          break;
        case MouseFlag::right_up:
          XTestFakeButtonEvent(display_, 3, False, CurrentTime);
          XFlush(display_);
          break;
        case MouseFlag::middle_down:
          XTestFakeButtonEvent(display_, 2, True, CurrentTime);
          XFlush(display_);
          break;
        case MouseFlag::middle_up:
          XTestFakeButtonEvent(display_, 2, False, CurrentTime);
          XFlush(display_);
          break;
        case MouseFlag::wheel_vertical: {
          if (remote_action.m.s > 0) {
            SimulateMouseWheel(4, remote_action.m.s);
          } else if (remote_action.m.s < 0) {
            SimulateMouseWheel(5, -remote_action.m.s);
          }
          break;
        }
        case MouseFlag::wheel_horizontal: {
          if (remote_action.m.s > 0) {
            SimulateMouseWheel(6, remote_action.m.s);
          } else if (remote_action.m.s < 0) {
            SimulateMouseWheel(7, -remote_action.m.s);
          }
          break;
        }
      }
      break;
    default:
      break;
  }

  return 0;
}

void MouseController::SetMousePosition(int x, int y) {
  XWarpPointer(display_, None, root_, 0, 0, 0, 0, x, y);
  XFlush(display_);
}

void MouseController::SimulateKeyDown(int kval) {
  XTestFakeKeyEvent(display_, kval, True, CurrentTime);
  XFlush(display_);
}

void MouseController::SimulateKeyUp(int kval) {
  XTestFakeKeyEvent(display_, kval, False, CurrentTime);
  XFlush(display_);
}

void MouseController::SimulateMouseWheel(int direction_button, int count) {
  for (int i = 0; i < count; ++i) {
    XTestFakeButtonEvent(display_, direction_button, True, CurrentTime);
    XTestFakeButtonEvent(display_, direction_button, False, CurrentTime);
  }
  XFlush(display_);
}
}  // namespace crossdesk