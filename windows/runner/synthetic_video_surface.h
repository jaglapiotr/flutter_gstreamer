#pragma once

#include <flutter/texture_registrar.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <cstdint>
#include <mutex>
#include <iostream>
#include <atomic>

struct FrameData {
    GstSample* sample;
    GstMapInfo map;
};

class SyntheticVideoSource {
public:
    SyntheticVideoSource(flutter::TextureRegistrar* registrar)
        : registrar_(registrar) {

        _putenv_s("GST_PLUGIN_PATH", "C:/personal_workspace/vcpkg/installed/x64-windows/debug/plugins/gstreamer");

       
        if(!gst_is_initialized()) {
            gst_init(nullptr,nullptr);
        }
       
        width_ = 1280;
        height_ = 720;

        std::string pipeline_str = 
            "videotestsrc pattern=ball ! "
            "video/x-raw,width=1280,height=720,framerate=30/1 ! "
            "videoconvert ! "
            "video/x-raw,format=BGRA ! "
            "appsink name=sink emit-signals=true sync=true";

        GError* error = nullptr;
        pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);

        if (error) {
            std::cerr << "!!! GStreamer Error: " << error->message << std::endl;
            g_error_free(error);
        }

        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
        g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
        gst_object_unref(sink);

        texture_ = std::make_unique<flutter::TextureVariant>(flutter::PixelBufferTexture(
            [this](size_t width, size_t height) -> const FlutterDesktopPixelBuffer* {
                return CopyPixelBuffer(width, height);
            }));

        texture_id_ = registrar_->RegisterTexture(texture_.get());
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    }

    virtual ~SyntheticVideoSource() {
        if (pipeline_) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
            gst_object_unref(pipeline_);
        }
        registrar_->UnregisterTexture(texture_id_);
        
        std::lock_guard lock(mutex_);
        if (next_frame_) CleanupFrame(next_frame_);
        if (last_processed_frame_) CleanupFrame(last_processed_frame_);
    }

    int64_t texture_id() const { return texture_id_; }

    const FlutterDesktopPixelBuffer* CopyPixelBuffer(size_t width, size_t height) {
        std::lock_guard lock(mutex_);
        if (next_frame_) {
            if (last_processed_frame_) {
                CleanupFrame(last_processed_frame_);
            }
            last_processed_frame_ = next_frame_;
            next_frame_ = nullptr;
        }

        if (!last_processed_frame_) {
            return nullptr;
        }

        flutter_pixel_buffer_.width = width_;
        flutter_pixel_buffer_.height = height_;
        flutter_pixel_buffer_.buffer = last_processed_frame_->map.data;
        
        // Important: Do NOT set release_callback here if you want to reuse the buffer.
        // In PixelBufferTexture, Flutter copies the data from buffer to the graphics card
        // inside the CopyPixelBuffer call, so the data only needs to be valid until the function exits.
        flutter_pixel_buffer_.release_callback = nullptr;
        flutter_pixel_buffer_.release_context = nullptr;

        return &flutter_pixel_buffer_;
    }

private:
    static void CleanupFrame(FrameData* frame) {
        if (frame) {
            gst_buffer_unmap(gst_sample_get_buffer(frame->sample), &frame->map);
            gst_sample_unref(frame->sample);
            delete frame;
        }
    }

    static GstFlowReturn OnNewSample(GstElement* sink, gpointer user_data) {
        auto self = static_cast<SyntheticVideoSource*>(user_data);
        GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        
        if (sample) {
            auto* frame = new FrameData();
            frame->sample = sample;
            GstBuffer* gst_buffer = gst_sample_get_buffer(sample);

            if (gst_buffer_map(gst_buffer, &frame->map, GST_MAP_READ)) {
                std::lock_guard lock(self->mutex_);
                
                // Jeśli kolejna klatka już czekała, a Flutter jej nie odebrał, zwalniamy ją.
                if (self->next_frame_) {
                    CleanupFrame(self->next_frame_);
                }
                
                self->next_frame_ = frame;
                self->registrar_->MarkTextureFrameAvailable(self->texture_id_);
            } else {
                delete frame;
                gst_sample_unref(sample);
            }
        }
        return GST_FLOW_OK;
    }

    flutter::TextureRegistrar* registrar_ = nullptr;
    std::unique_ptr<flutter::TextureVariant> texture_;
    int64_t texture_id_ = -1;
    GstElement* pipeline_ = nullptr;
    
    size_t width_ = 0;
    size_t height_ = 0;
    
    // Podwójne buforowanie po stronie C++:
    FrameData* next_frame_ = nullptr;           // Klatka, która właśnie przyszła z GStreamera
    FrameData* last_processed_frame_ = nullptr; // Ostatnia klatka, którą pokazaliśmy Flutterowi
    
    FlutterDesktopPixelBuffer flutter_pixel_buffer_ = {};
    mutable std::mutex mutex_;
};