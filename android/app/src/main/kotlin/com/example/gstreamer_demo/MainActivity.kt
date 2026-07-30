package com.example.gstreamer_demo

import android.os.Bundle
import android.view.Surface
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import io.flutter.view.TextureRegistry

import org.freedesktop.gstreamer.GStreamer

class MainActivity: FlutterActivity() {
  companion object {
    init {
      // gstreamer_android_texture (built from src/main/cpp/CMakeLists.txt)
      // depends on gstreamer_android, so it must be loaded first.
      System.loadLibrary("gstreamer_android")
      System.loadLibrary("gstreamer_android_texture")
    }

    // Must match the container size in lib/main.dart and mirrors the desktop
    // runners' synthetic test pipeline resolution.
    private const val VIDEO_WIDTH = 640
    private const val VIDEO_HEIGHT = 360
  }

  private external fun nativeLogGStreamerInit()

  // Bridges to native-lib.cpp: starts/stops a GStreamer test pipeline that
  // renders into the Surface backing a Flutter SurfaceTexture.
  private external fun nativeCreateVideoSource(surface: Surface, width: Int, height: Int): Long
  private external fun nativeDestroyVideoSource(handle: Long)

  private var videoSourceHandle: Long = 0
  private var surfaceTextureEntry: TextureRegistry.SurfaceTextureEntry? = null
  private var videoSurface: Surface? = null

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    GStreamer.init(this)
    nativeLogGStreamerInit()
  }

  override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
    super.configureFlutterEngine(flutterEngine)

    // Channel and method names mirror the desktop runners (Windows
    // flutter_window.cpp / Linux my_application.cc) and lib/main.dart.
    MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "win_texture_poc")
      .setMethodCallHandler { call, result ->
        when (call.method) {
          "createTexture" -> result.success(createTexture(flutterEngine))
          else -> result.notImplemented()
        }
      }
  }

  // Registers a Flutter texture backed by a SurfaceTexture and points the
  // GStreamer test pipeline at it. Returns the Flutter texture id consumed by
  // the Texture widget in lib/main.dart.
  private fun createTexture(flutterEngine: FlutterEngine): Long {
    val entry = flutterEngine.renderer.createSurfaceTexture()
    val surfaceTexture = entry.surfaceTexture()
    surfaceTexture.setDefaultBufferSize(VIDEO_WIDTH, VIDEO_HEIGHT)
    val surface = Surface(surfaceTexture)

    videoSourceHandle = nativeCreateVideoSource(surface, VIDEO_WIDTH, VIDEO_HEIGHT)
    surfaceTextureEntry = entry
    videoSurface = surface

    return entry.id()
  }

  override fun onDestroy() {
    if (videoSourceHandle != 0L) {
      nativeDestroyVideoSource(videoSourceHandle)
      videoSourceHandle = 0
    }
    videoSurface?.release()
    videoSurface = null
    surfaceTextureEntry?.release()
    surfaceTextureEntry = null

    super.onDestroy()
  }
}
