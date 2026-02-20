#pragma once

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <flutter_linux/flutter_linux.h>

#include <iostream>

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
            "video/x-raw,width=1280,height=720,framerate=30/1 ! "
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

        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
        g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
        gst_object_unref(sink);

        if(gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "!!! Couldn't set pipeline to playing state" << std::endl;
        }

        (void)registrar_;
        (void)texture_;

    }

    int64_t texture_id() const {
        return texture_id_;
    }



private:
    GstElement* pipeline_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    FlTextureRegistrar* registrar_ = nullptr;
    FlTexture* texture_ = nullptr;
    int64_t texture_id_ = -1;

};