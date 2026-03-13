#pragma once

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <flutter_linux/flutter_linux.h>

#include <iostream>

struct FrameData {
    GstSample* sample;
    GstMapInfo map;
};

struct _SyntheticTextureClass {
    FlPixelBufferTextureClass parent_class;
};

struct SyntheticTexturePrivate {
    int64_t texture_id = 0;
    int32_t video_width = 0;
    int32_t video_height = 0;
    FrameData* last_processed_frame = nullptr;
    FrameData* next_frame = nullptr;
};

static void CleanupFrame(FrameData* frame) {
    if(frame) {
        gst_buffer_unmap(gst_sample_get_buffer(frame->sample), &frame->map);
        gst_sample_unref(frame->sample);
        delete frame;
    }
}

G_DECLARE_DERIVABLE_TYPE(SyntheticTexture, synthetic_texture, MY_OPENGL, 
    SYNTHETIC_TEXTURE, FlPixelBufferTexture)

G_DEFINE_TYPE_WITH_CODE(SyntheticTexture, synthetic_texture, fl_pixel_buffer_texture_get_type(), G_ADD_PRIVATE(SyntheticTexture))

static SyntheticTexture* synthetic_texture_new() {
    return MY_OPENGL_SYNTHETIC_TEXTURE(g_object_new(synthetic_texture_get_type(), nullptr));
}

static gboolean synthetic_texture_copy_pixels(FlPixelBufferTexture* texture, 
    const uint8_t** out_buffer, uint32_t* width, uint32_t* height, GError** error) {


    auto synthetic_texture_private = (SyntheticTexturePrivate*) synthetic_texture_get_instance_private(MY_OPENGL_SYNTHETIC_TEXTURE(texture));
    if(synthetic_texture_private->next_frame) {
        if(synthetic_texture_private->last_processed_frame) {
            CleanupFrame(synthetic_texture_private->last_processed_frame);
        }
        synthetic_texture_private->last_processed_frame = synthetic_texture_private->next_frame;
        synthetic_texture_private->next_frame = nullptr;
    }

    *out_buffer = synthetic_texture_private->last_processed_frame->map.data;
    // *width = synthetic_texture_private->video_width;
    // *height = synthetic_texture_private->video_height;
    *width = 640;
    *height = 360;

    return TRUE;
}

static void synthetic_texture_class_init(SyntheticTextureClass* klass) {
    FL_PIXEL_BUFFER_TEXTURE_CLASS(klass)->copy_pixels = synthetic_texture_copy_pixels;
}

static void synthetic_texture_init(SyntheticTexture* self) {}


class SyntheticVideoSource
{
public:
    SyntheticVideoSource(FlTextureRegistrar* registrar) : registrar_(registrar) {

        if(!gst_is_initialized()) {
            gst_init(nullptr, nullptr);
        }

        width_ = 1280;
        height_ = 720;

        std::string pipeline_str = 
            "videotestsrc ! "
            "video/x-raw,width=640,height=360,framerate=30/1 ! "
            "videoconvert ! "
            "video/x-raw,format=BGRA ! "
            "appsink name=sink emit-signals=true sync=true";

        std::cout << "KANAPKA Tutaj przychodzi 1" << std::endl;
        GError* error = nullptr;
        pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);

        if(error) {
            std::cerr << "!!! GStreamer Error: " << error->message << std::endl;
            g_error_free(error);
        }


        synthetic_texture_ = synthetic_texture_new();

        FL_PIXEL_BUFFER_TEXTURE_GET_CLASS(synthetic_texture_)->copy_pixels = synthetic_texture_copy_pixels;
        fl_texture_registrar_register_texture(registrar_, FL_TEXTURE(synthetic_texture_));



        // auto synthetic_texture_private = (SyntheticTexturePrivate*)synthetic_texture_get_instance_private(synthetic_texture_);
        // synthetic_texture_private->texture_id = reinterpret_cast<int64_t>(FL_TEXTURE(synthetic_texture_));
        // std::cout << "KANAPKA Texture ID: " << synthetic_texture_private->texture_id << std::endl;
        synthetic_texture_private_ = (SyntheticTexturePrivate*)synthetic_texture_get_instance_private(synthetic_texture_);
        synthetic_texture_private_->texture_id = reinterpret_cast<int64_t>(FL_TEXTURE(synthetic_texture_));

        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
        g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
        gst_object_unref(sink);

        if(gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "!!! Couldn't set pipeline to playing state" << std::endl;
        }

        (void)texture_id_;
    }

    int64_t texture_id() const {
        return ((SyntheticTexturePrivate*)synthetic_texture_get_instance_private(synthetic_texture_))->texture_id;
    }

    SyntheticTexture* syntheticTexture() const {
        return synthetic_texture_;
    }



private:

    static GstFlowReturn OnNewSample(GstElement* sink, gpointer user_data) {
        auto self = static_cast<SyntheticVideoSource*>(user_data);
        GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

        if(sample) {
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            auto* frame = new FrameData();
            frame->sample = sample;
            if(buffer != NULL) {
                gst_buffer_map(buffer, &frame->map, GST_MAP_READ);

                // GstVideoInfo info;
                // GstCaps* sampleCaps = gst_sample_get_caps(sample);
                // gst_video_info_from_caps(&info, sampleCaps);

                auto synthetic_texture_private = (SyntheticTexturePrivate*)synthetic_texture_get_instance_private(self->synthetic_texture_);
                // synthetic_texture_private->buffer = (uint8_t*)frame.data;
                // synthetic_texture_private->video_width = info.width;
                // synthetic_texture_private->video_height = info.height;
                if(synthetic_texture_private->next_frame) {
                    CleanupFrame(synthetic_texture_private->next_frame);
                }
                // synthetic_texture_private->video_width = info.width;
                // synthetic_texture_private->video_height = info.height;
                synthetic_texture_private->next_frame = frame;
                fl_texture_registrar_mark_texture_frame_available(self->registrar_, FL_TEXTURE(self->synthetic_texture_));

                // gst_caps_unref(sampleCaps);
            } else {
                delete frame;
                gst_sample_unref(sample);
            }
        }

        return GST_FLOW_OK;
    }


    GstElement* pipeline_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    FlTextureRegistrar* registrar_ = nullptr;
    // FlTexture* texture_ = nullptr;
    // FlPixelBufferTexture* pixelBuffer_ = nullptr;
    SyntheticTexturePrivate* synthetic_texture_private_ = nullptr;
    SyntheticTexture* synthetic_texture_ = nullptr;
    int64_t texture_id_ = -1;

};