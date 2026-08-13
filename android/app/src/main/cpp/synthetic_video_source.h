#pragma once

#include <android/native_window.h>
#include <android/log.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>

#define LOG_TAG "SyntheticVideoSource"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Mirrors the Windows (synthetic_video_surface.h) / Linux (synthetic_video_source.h)
// implementations: a videotestsrc pipeline is decoded to raw RGBA frames via appsink,
// and each frame is copied into the ANativeWindow that backs the Flutter SurfaceTexture.
class SyntheticVideoSource {
 public:
  SyntheticVideoSource(ANativeWindow* window, int width, int height)
      : window_(window), width_(width), height_(height) {
    ANativeWindow_acquire(window_);
    ANativeWindow_setBuffersGeometry(window_, width_, height_,
                                      WINDOW_FORMAT_RGBA_8888);

    if (!gst_is_initialized()) {
      gst_init(nullptr, nullptr);
    }

    std::string pipeline_str =
        "rtspsrc location=rtsp://192.168.16.254:9001/stream latency=200 protocols=tcp "
        "! rtph264depay "
        "! video/x-h264,stream-format=(string)byte-stream,alignment=(string)nal "
        "! decodebin "
        "! videoconvert "
        "! video/x-raw,format=RGBA "
        "! appsink name=sink emit-signals=true sync=true max-buffers=1 drop=true";

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
    if (error) {
      LOGE("KANAPKA pipeline parse failed: %s", error->message);
      g_error_free(error);
      return;
    }
    LOGI("KANAPKA pipeline created successfully: %s", pipeline_str.c_str());

    GstBus* bus = gst_element_get_bus(pipeline_);
    gst_bus_set_sync_handler(bus, &SyntheticVideoSource::OnBusMessage, this, nullptr);
    gst_object_unref(bus);

    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
    gst_object_unref(sink);

    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
      LOGE("KANAPKA couldn't set pipeline to playing state");
    } else {
      LOGI("KANAPKA pipeline state change to playing requested successfully");
    }
  }

  ~SyntheticVideoSource() {
    if (pipeline_) {
      gst_element_set_state(pipeline_, GST_STATE_NULL);
      gst_object_unref(pipeline_);
      pipeline_ = nullptr;
    }
    if (window_) {
      ANativeWindow_release(window_);
      window_ = nullptr;
    }
  }

 private:
  static GstBusSyncReply OnBusMessage(GstBus* bus, GstMessage* message, gpointer user_data) {
    auto* self = static_cast<SyntheticVideoSource*>(user_data);
    switch (GST_MESSAGE_TYPE(message)) {
      case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &err, &debug);
        LOGE("KANAPKA pipeline error from %s: %s (%s)", GST_OBJECT_NAME(message->src),
             err->message, debug ? debug : "no debug info");
        g_clear_error(&err);
        g_free(debug);
        break;
      }
      case GST_MESSAGE_WARNING: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_warning(message, &err, &debug);
        LOGI("KANAPKA pipeline warning from %s: %s", GST_OBJECT_NAME(message->src), err->message);
        g_clear_error(&err);
        g_free(debug);
        break;
      }
      case GST_MESSAGE_EOS:
        LOGI("KANAPKA pipeline reached end of stream");
        break;
      case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->pipeline_)) {
          GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
          LOGI("KANAPKA pipeline state changed: %s -> %s",
               gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        break;
      default:
        break;
    }
    return GST_BUS_PASS;
  }

  static GstFlowReturn OnNewSample(GstElement* sink, gpointer user_data) {
    auto* self = static_cast<SyntheticVideoSource*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
      return GST_FLOW_OK;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      self->CopyFrameToWindow(map.data, map.size);
      gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }

  void CopyFrameToWindow(const uint8_t* src, size_t src_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(window_, &buffer, nullptr) != 0) {
      return;
    }

    const int row_bytes = width_ * 4;
    const int rows = std::min(buffer.height, height_);
    auto* dst = static_cast<uint8_t*>(buffer.bits);
    const int dst_stride_bytes = buffer.stride * 4;

    for (int y = 0; y < rows; y++) {
      size_t src_offset = static_cast<size_t>(y) * row_bytes;
      if (src_offset + row_bytes > src_size) break;
      memcpy(dst + y * dst_stride_bytes, src + src_offset, row_bytes);
    }

    ANativeWindow_unlockAndPost(window_);
  }

  ANativeWindow* window_ = nullptr;
  GstElement* pipeline_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  std::mutex mutex_;
};
