package com.example.gstreamer_demo

import android.os.Bundle
import io.flutter.embedding.android.FlutterActivity

import org.freedesktop.gstreamer.GStreamer

class MainActivity: FlutterActivity() {
  companion object {
    init {
      // gstreamer_android_texture (built from src/main/cpp/CMakeLists.txt)
      // depends on gstreamer_android, so it must be loaded first.
      System.loadLibrary("gstreamer_android")
      System.loadLibrary("gstreamer_android_texture")
    }
  }

  private external fun nativeLogGStreamerInit()

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    GStreamer.init(this)
    nativeLogGStreamerInit()
  }
}
