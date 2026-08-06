#include "synthetic_video_source.h"

#include <gst/app/gstappsink.h>

#include <cstdio>
#include <cstring>
#include <string>

// GStreamer linking (headers/libs, framework search paths, static plugin
// registration, etc.) is intentionally NOT set up here - see the CMakeLists.txt
// placeholder in this directory. This file assumes <gst/gst.h> and the appsink
// headers already resolve once that linking is in place.

SyntheticVideoSource::SyntheticVideoSource(int width, int height,
                                            std::function<void()> on_frame_available)
    : width_(width), height_(height), on_frame_available_(std::move(on_frame_available)) {
  if (!gst_is_initialized()) {
    gst_init(nullptr, nullptr);
  }

  std::string pipeline_str =
      "videotestsrc ! "
      "video/x-raw,width=" + std::to_string(width_) +
      ",height=" + std::to_string(height_) + ",framerate=30/1 ! "
      "videoconvert ! "
      "video/x-raw,format=BGRA ! "
      "appsink name=sink emit-signals=true sync=true max-buffers=1 drop=true";

  GError* error = nullptr;
  pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
  if (error) {
    fprintf(stderr, "SyntheticVideoSource: GStreamer error: %s\n", error->message);
    g_error_free(error);
    return;
  }

  GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
  g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
  gst_object_unref(sink);

  if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    fprintf(stderr, "SyntheticVideoSource: couldn't set pipeline to playing state\n");
  }
}

SyntheticVideoSource::~SyntheticVideoSource() {
  if (pipeline_ != nullptr) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (latest_pixel_buffer_ != nullptr) {
    CVPixelBufferRelease(latest_pixel_buffer_);
    latest_pixel_buffer_ = nullptr;
  }
}

CVPixelBufferRef SyntheticVideoSource::CopyLatestPixelBuffer() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (latest_pixel_buffer_ == nullptr) {
    return nullptr;
  }
  return CVPixelBufferRetain(latest_pixel_buffer_);
}

GstFlowReturn SyntheticVideoSource::OnNewSample(GstElement* sink, gpointer user_data) {
  auto* self = static_cast<SyntheticVideoSource*>(user_data);
  GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
  if (sample == nullptr) {
    return GST_FLOW_OK;
  }

  GstBuffer* buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;
  if (buffer != nullptr && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
    self->CopyFrameToPixelBuffer(map.data, map.size);
    gst_buffer_unmap(buffer, &map);
  }

  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

void SyntheticVideoSource::CopyFrameToPixelBuffer(const uint8_t* src, size_t src_size) {
  CVPixelBufferRef pixel_buffer = nullptr;
  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, width_, height_,
                                         kCVPixelFormatType_32BGRA, nullptr, &pixel_buffer);
  if (status != kCVReturnSuccess || pixel_buffer == nullptr) {
    return;
  }

  CVPixelBufferLockBaseAddress(pixel_buffer, 0);
  auto* dst = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixel_buffer));
  const size_t dst_stride = CVPixelBufferGetBytesPerRow(pixel_buffer);
  const size_t row_bytes = static_cast<size_t>(width_) * 4;

  for (int y = 0; y < height_; y++) {
    size_t src_offset = static_cast<size_t>(y) * row_bytes;
    if (src_offset + row_bytes > src_size) break;
    memcpy(dst + y * dst_stride, src + src_offset, row_bytes);
  }
  CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latest_pixel_buffer_ != nullptr) {
      CVPixelBufferRelease(latest_pixel_buffer_);
    }
    latest_pixel_buffer_ = pixel_buffer;
  }

  if (on_frame_available_) {
    on_frame_available_();
  }
}
