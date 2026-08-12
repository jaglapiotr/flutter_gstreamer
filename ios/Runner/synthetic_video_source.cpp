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

  fprintf(stderr, "KANAPKA SyntheticVideoSource: launching pipeline: %s\n", pipeline_str.c_str());

  GError* error = nullptr;
  pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
  if (error) {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: GStreamer error: %s\n", error->message);
    g_error_free(error);
    return;
  }
  if (pipeline_ == nullptr) {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: gst_parse_launch returned null pipeline (no error set)\n");
    return;
  }
  fprintf(stderr, "KANAPKA SyntheticVideoSource: pipeline parsed successfully\n");

  GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
  if (sink == nullptr) {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: could not find appsink named 'sink' in pipeline\n");
    return;
  }
  g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
  gst_object_unref(sink);

  bus_thread_running_ = true;
  bus_thread_ = std::thread(&SyntheticVideoSource::BusLoop, this);

  GstStateChangeReturn state_ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  fprintf(stderr, "KANAPKA SyntheticVideoSource: set_state(PLAYING) returned %s\n",
          gst_element_state_change_return_get_name(state_ret));
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: couldn't set pipeline to playing state\n");
  }
}

SyntheticVideoSource::~SyntheticVideoSource() {
  bus_thread_running_ = false;
  if (bus_thread_.joinable()) {
    bus_thread_.join();
  }

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
    fprintf(stderr, "KANAPKA SyntheticVideoSource: OnNewSample got null sample\n");
    return GST_FLOW_OK;
  }

  GstBuffer* buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;
  if (buffer != nullptr && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
    int count = ++self->frame_count_;
    if (count <= 5 || count % 30 == 0) {
      fprintf(stderr, "KANAPKA SyntheticVideoSource: received frame #%d, %zu bytes\n", count, map.size);
    }
    self->CopyFrameToPixelBuffer(map.data, map.size);
    gst_buffer_unmap(buffer, &map);
  } else {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: OnNewSample - null buffer or map failed\n");
  }

  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

void SyntheticVideoSource::BusLoop() {
  GstBus* bus = gst_element_get_bus(pipeline_);
  const auto watched_types = static_cast<GstMessageType>(
      GST_MESSAGE_ERROR | GST_MESSAGE_WARNING | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);

  while (bus_thread_running_) {
    GstMessage* msg = gst_bus_timed_pop_filtered(bus, 200 * GST_MSECOND, watched_types);
    if (msg == nullptr) {
      continue;
    }

    switch (GST_MESSAGE_TYPE(msg)) {
      case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(msg, &err, &debug);
        fprintf(stderr, "KANAPKA SyntheticVideoSource: [BUS ERROR] %s (debug: %s)\n",
                err ? err->message : "unknown", debug ? debug : "none");
        g_clear_error(&err);
        g_free(debug);
        break;
      }
      case GST_MESSAGE_WARNING: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_warning(msg, &err, &debug);
        fprintf(stderr, "KANAPKA SyntheticVideoSource: [BUS WARNING] %s (debug: %s)\n",
                err ? err->message : "unknown", debug ? debug : "none");
        g_clear_error(&err);
        g_free(debug);
        break;
      }
      case GST_MESSAGE_EOS:
        fprintf(stderr, "KANAPKA SyntheticVideoSource: [BUS] End-of-stream\n");
        break;
      case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
          GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
          fprintf(stderr, "KANAPKA SyntheticVideoSource: [BUS] pipeline state: %s -> %s (pending: %s)\n",
                  gst_element_state_get_name(old_state), gst_element_state_get_name(new_state),
                  gst_element_state_get_name(pending_state));
        }
        break;
      default:
        break;
    }
    gst_message_unref(msg);
  }

  gst_object_unref(bus);
}

void SyntheticVideoSource::CopyFrameToPixelBuffer(const uint8_t* src, size_t src_size) {
  CVPixelBufferRef pixel_buffer = nullptr;
  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, width_, height_,
                                         kCVPixelFormatType_32BGRA, nullptr, &pixel_buffer);
  if (status != kCVReturnSuccess || pixel_buffer == nullptr) {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: CVPixelBufferCreate failed, status=%d\n", (int)status);
    return;
  }

  CVPixelBufferLockBaseAddress(pixel_buffer, 0);
  auto* dst = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixel_buffer));
  const size_t dst_stride = CVPixelBufferGetBytesPerRow(pixel_buffer);
  const size_t row_bytes = static_cast<size_t>(width_) * 4;
  const size_t expected_size = row_bytes * static_cast<size_t>(height_);
  if (frame_count_ <= 5 && src_size < expected_size) {
    fprintf(stderr, "KANAPKA SyntheticVideoSource: frame buffer smaller than expected: got %zu bytes, expected %zu\n",
            src_size, expected_size);
  }

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
