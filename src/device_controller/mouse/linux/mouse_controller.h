/*
 * @Author: DI JUNKUN
 * @Date: 2025-05-07
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _MOUSE_CONTROLLER_H_
#define _MOUSE_CONTROLLER_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>

#include <vector>

#include "device_controller.h"

namespace crossdesk {

class MouseController : public DeviceController {
 public:
  MouseController();
  virtual ~MouseController();

 public:
  virtual int Init(std::vector<DisplayInfo> display_info_list);
  virtual int Destroy();
  virtual int SendMouseCommand(RemoteAction remote_action, int display_index);

 private:
  void SimulateKeyDown(int kval);
  void SimulateKeyUp(int kval);
  void SetMousePosition(int x, int y);
  void SimulateMouseWheel(int direction_button, int count);

  Display* display_ = nullptr;
  Window root_ = 0;
  std::vector<DisplayInfo> display_info_list_;
  int screen_width_ = 0;
  int screen_height_ = 0;
};
}  // namespace crossdesk
#endif