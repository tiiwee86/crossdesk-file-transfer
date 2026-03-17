#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "rd_log.h"
#include "speaker_capturer_macosx.h"

namespace crossdesk {
class SpeakerCapturerMacosx;
}

@interface SpeakerCaptureDelegate : NSObject <SCStreamDelegate, SCStreamOutput>
@property(nonatomic, assign) crossdesk::SpeakerCapturerMacosx* owner;
- (instancetype)initWithOwner:(crossdesk::SpeakerCapturerMacosx*)owner;
@end

@implementation SpeakerCaptureDelegate
- (instancetype)initWithOwner:(crossdesk::SpeakerCapturerMacosx*)owner {
  self = [super init];
  if (self) {
    _owner = owner;
  }
  return self;
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  if (type != SCStreamOutputTypeAudio) return;

  CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
  size_t length = CMBlockBufferGetDataLength(blockBuffer);
  char* dataPtr = NULL;
  CMBlockBufferGetDataPointer(blockBuffer, 0, NULL, NULL, &dataPtr);
  CMAudioFormatDescriptionRef formatDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
  const AudioStreamBasicDescription* asbd =
      CMAudioFormatDescriptionGetStreamBasicDescription(formatDesc);

  if (_owner->cb_ && dataPtr && length > 0 && asbd) {
    std::vector<short> out_pcm16;
    if (asbd->mFormatFlags & kAudioFormatFlagIsFloat) {
      int channels = asbd->mChannelsPerFrame;
      int samples = (int)(length / sizeof(float));
      float* floatData = (float*)dataPtr;
      std::vector<short> pcm16(samples);
      for (int i = 0; i < samples; ++i) {
        float v = floatData[i];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        pcm16[i] = (short)(v * 32767.0f);
      }

      if (channels > 1) {
        int mono_samples = samples / channels;
        out_pcm16.resize(mono_samples);
        for (int i = 0; i < mono_samples; ++i) {
          int sum = 0;
          for (int c = 0; c < channels; ++c) {
            sum += pcm16[i * channels + c];
          }
          out_pcm16[i] = sum / channels;
        }
      } else {
        out_pcm16 = std::move(pcm16);
      }
    } else if (asbd->mBitsPerChannel == 16) {
      int channels = asbd->mChannelsPerFrame;
      int samples = (int)(length / 2);
      short* src = (short*)dataPtr;
      if (channels > 1) {
        int mono_samples = samples / channels;
        out_pcm16.resize(mono_samples);
        for (int i = 0; i < mono_samples; ++i) {
          int sum = 0;
          for (int c = 0; c < channels; ++c) {
            sum += src[i * channels + c];
          }
          out_pcm16[i] = sum / channels;
        }
      } else {
        out_pcm16.assign(src, src + samples);
      }
    }

    size_t frame_bytes = 960;  // 480 * 2
    size_t total_bytes = out_pcm16.size() * sizeof(short);
    unsigned char* p = (unsigned char*)out_pcm16.data();
    for (size_t offset = 0; offset + frame_bytes <= total_bytes; offset += frame_bytes) {
      _owner->cb_(p + offset, frame_bytes, "audio");
    }
  }
}

@end

namespace crossdesk {

class SpeakerCapturerMacosx::Impl {
 public:
  SCStreamConfiguration* config = nil;
  SCStream* stream = nil;
  SpeakerCaptureDelegate* delegate = nil;
  dispatch_queue_t queue = nil;
  SCShareableContent* content = nil;
  SCDisplay* mainDisplay = nil;

  ~Impl() {
    if (stream) {
      [stream stopCaptureWithCompletionHandler:^(NSError* _Nullable error){
      }];
      stream = nil;
    }
    delegate = nil;
    if (queue) {
      queue = nil;
    }
    content = nil;
    mainDisplay = nil;
    config = nil;
  }
};

SpeakerCapturerMacosx::SpeakerCapturerMacosx() {
  impl_ = new Impl();
  inited_ = false;
  cb_ = nullptr;
}

SpeakerCapturerMacosx::~SpeakerCapturerMacosx() {
  Destroy();
  delete impl_;
  impl_ = nullptr;
}

int SpeakerCapturerMacosx::Init(speaker_data_cb cb) {
  if (inited_) {
    return 0;
  }
  cb_ = cb;

  impl_->config = [[SCStreamConfiguration alloc] init];
  impl_->config.capturesAudio = YES;
  impl_->config.sampleRate = 48000;
  impl_->config.channelCount = 1;

  dispatch_semaphore_t sema = dispatch_semaphore_create(0);
  __block NSError* error = nil;
  [SCShareableContent
      getShareableContentWithCompletionHandler:^(SCShareableContent* c, NSError* e) {
        impl_->content = c;
        error = e;
        dispatch_semaphore_signal(sema);
      }];
  dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

  if (error || !impl_->content) {
    LOG_ERROR("Failed to get shareable content: {}",
              std::string([error.localizedDescription UTF8String]));
    return -1;
  }

  CGDirectDisplayID mainDisplayId = CGMainDisplayID();
  impl_->mainDisplay = nil;
  for (SCDisplay* d in impl_->content.displays) {
    if (d.displayID == mainDisplayId) {
      impl_->mainDisplay = d;
      break;
    }
  }
  if (!impl_->mainDisplay) {
    LOG_ERROR("Main display not found");
    return -1;
  }

  if (!impl_->queue) {
    impl_->queue = dispatch_queue_create("SpeakerAudio.Queue", DISPATCH_QUEUE_SERIAL);
  }

  inited_ = true;
  return 0;
}

int SpeakerCapturerMacosx::Start() {
  if (!inited_) {
    return -1;
  }

  if (impl_->stream) {
    dispatch_semaphore_t semaStop = dispatch_semaphore_create(0);
    [impl_->stream stopCaptureWithCompletionHandler:^(NSError* error) {
      dispatch_semaphore_signal(semaStop);
    }];
    dispatch_semaphore_wait(semaStop, DISPATCH_TIME_FOREVER);
    impl_->stream = nil;
    impl_->delegate = nil;
  }

  impl_->delegate = [[SpeakerCaptureDelegate alloc] initWithOwner:this];
  SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:impl_->mainDisplay
                                                    excludingWindows:@[]];
  impl_->stream = [[SCStream alloc] initWithFilter:filter
                                     configuration:impl_->config
                                          delegate:impl_->delegate];

  NSError* addOutputError = nil;
  BOOL ok = [impl_->stream addStreamOutput:impl_->delegate
                                      type:SCStreamOutputTypeAudio
                        sampleHandlerQueue:impl_->queue
                                     error:&addOutputError];
  if (!ok || addOutputError) {
    LOG_ERROR("addStreamOutput error: {}",
              std::string([addOutputError.localizedDescription UTF8String]));
    impl_->stream = nil;
    impl_->delegate = nil;
    return -1;
  }

  dispatch_semaphore_t semaStart = dispatch_semaphore_create(0);
  __block int ret = 0;
  [impl_->stream startCaptureWithCompletionHandler:^(NSError* _Nullable error) {
    if (error) {
      LOG_ERROR("startCaptureWithCompletionHandler error: {}",
                std::string([error.localizedDescription UTF8String]));
      ret = -1;
    }
    dispatch_semaphore_signal(semaStart);
  }];
  dispatch_semaphore_wait(semaStart, DISPATCH_TIME_FOREVER);

  return ret;
}

int SpeakerCapturerMacosx::Stop() {
  if (!inited_) return -1;
  if (!impl_->stream) return -1;

  dispatch_semaphore_t sema = dispatch_semaphore_create(0);
  [impl_->stream stopCaptureWithCompletionHandler:^(NSError* error) {
    if (error) {
      LOG_ERROR("stopCaptureWithCompletionHandler error: {}",
                std::string([error.localizedDescription UTF8String]));
    }
    dispatch_semaphore_signal(sema);
  }];
  dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

  impl_->stream = nil;
  impl_->delegate = nil;

  return 0;
}

int SpeakerCapturerMacosx::Destroy() {
  Stop();
  cb_ = nullptr;

  if (impl_) {
    impl_->config = nil;
    impl_->content = nil;
    impl_->mainDisplay = nil;
    if (impl_->queue) {
      impl_->queue = nil;
    }
  }
  inited_ = false;
  return 0;
}

int SpeakerCapturerMacosx::Pause() { return 0; }

int SpeakerCapturerMacosx::Resume() { return Start(); }
}  // namespace crossdesk