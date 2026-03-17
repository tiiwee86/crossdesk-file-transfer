#include "mouse_controller.h"

#include "rd_log.h"

namespace crossdesk {

MouseController::MouseController() {}

MouseController::~MouseController() {}

int MouseController::Init(std::vector<DisplayInfo> display_info_list) {
  display_info_list_ = display_info_list;

  return 0;
}

int MouseController::Destroy() { return 0; }

int MouseController::SendMouseCommand(RemoteAction remote_action,
                                      int display_index) {
  INPUT ip;

  if (remote_action.type == ControlType::mouse) {
    ip.type = INPUT_MOUSE;
    ip.mi.dx =
        (LONG)(remote_action.m.x * display_info_list_[display_index].width) +
        display_info_list_[display_index].left;
    ip.mi.dy =
        (LONG)(remote_action.m.y * display_info_list_[display_index].height) +
        display_info_list_[display_index].top;

    switch (remote_action.m.flag) {
      case MouseFlag::left_down:
        ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
        break;
      case MouseFlag::left_up:
        ip.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
        break;
      case MouseFlag::right_down:
        ip.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_ABSOLUTE;
        break;
      case MouseFlag::right_up:
        ip.mi.dwFlags = MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_ABSOLUTE;
        break;
      case MouseFlag::middle_down:
        ip.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_ABSOLUTE;
        break;
      case MouseFlag::middle_up:
        ip.mi.dwFlags = MOUSEEVENTF_MIDDLEUP | MOUSEEVENTF_ABSOLUTE;
        break;
      case MouseFlag::wheel_vertical:
        ip.mi.dwFlags = MOUSEEVENTF_WHEEL;
        ip.mi.mouseData = remote_action.m.s * 120;
        break;
      case MouseFlag::wheel_horizontal:
        ip.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        ip.mi.mouseData = remote_action.m.s * 120;
        break;
      default:
        ip.mi.dwFlags = MOUSEEVENTF_MOVE;
        break;
    }

    ip.mi.time = 0;

    SetCursorPos(ip.mi.dx, ip.mi.dy);

    if (ip.mi.dwFlags != MOUSEEVENTF_MOVE) {
      SendInput(1, &ip, sizeof(INPUT));
    }
  }

  return 0;
}
}  // namespace crossdesk