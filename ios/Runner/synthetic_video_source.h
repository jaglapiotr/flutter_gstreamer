#pragma once

#include <CoreVideo/CVPixelBuffer.h>
#include <gst/gst.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

// Mirrors the Android (synthetic_video_source.h) / Windows (synthetic_video_surface.h) /
// Linux (synthetic_video_source.h) implementations: a videotestsrc pipeline is decoded to
// raw BGRA frames via appsink, and each frame is copied into a CVPixelBuffer.
//
// Kept as plain C++ (no Objective-C) so it builds as a normal .cpp translation unit;
// GStreamerVideoTexture.mm is the thin Objective-C++ shim that exposes this through the
// FlutterTexture protocol, mirroring how native-lib.cpp bridges the Android version to JNI.
class SyntheticVideoSource {
 public:
  // on_frame_available is invoked (off the main thread, from the GStreamer streaming
  // thread) each time a new frame has been copied and is ready via CopyLatestPixelBuffer.
  SyntheticVideoSource(int width, int height, std::function<void()> on_frame_available);
  ~SyntheticVideoSource();

  // Returns a retained copy of the latest frame (caller must CVPixelBufferRelease), or
  // nullptr if no frame has arrived yet.
  CVPixelBufferRef CopyLatestPixelBuffer();

 private:
  static GstFlowReturn OnNewSample(GstElement* sink, gpointer user_data);
  void CopyFrameToPixelBuffer(const uint8_t* src, size_t src_size);
  // Polls the pipeline's GstBus and logs ERROR/WARNING/EOS/STATE_CHANGED messages,
  // since we don't run a GMainLoop that would otherwise dispatch them.
  void BusLoop();

  GstElement* pipeline_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  std::function<void()> on_frame_available_;
  std::mutex mutex_;
  CVPixelBufferRef latest_pixel_buffer_ = nullptr;
  std::atomic<int> frame_count_{0};
  std::atomic<bool> bus_thread_running_{false};
  std::thread bus_thread_;
};
