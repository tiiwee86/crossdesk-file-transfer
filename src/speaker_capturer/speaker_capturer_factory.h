/*
 * @Author: DI JUNKUN
 * @Date: 2024-07-22
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SPEAKER_CAPTURER_FACTORY_H_
#define _SPEAKER_CAPTURER_FACTORY_H_

#ifdef _WIN32
#include "speaker_capturer_wasapi.h"
#elif __linux__
#include "speaker_capturer_linux.h"
#elif __APPLE__
#include "speaker_capturer_macosx.h"
#endif

namespace crossdesk {

class SpeakerCapturerFactory {
 public:
  virtual ~SpeakerCapturerFactory() {}

 public:
  SpeakerCapturer* Create() {
#ifdef _WIN32
    return new SpeakerCapturerWasapi();
#elif __linux__
    return new SpeakerCapturerLinux();
#elif __APPLE__
    return new SpeakerCapturerMacosx();
#else
    return nullptr;
#endif
  }
};
}  // namespace crossdesk
#endif