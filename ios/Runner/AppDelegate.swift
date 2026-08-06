import Flutter
import UIKit

@main
@objc class AppDelegate: FlutterAppDelegate {
  // Must match the container size in lib/main.dart and mirrors the other
  // runners' (Android MainActivity.kt, Windows flutter_window.cpp, Linux
  // my_application.cc) synthetic test pipeline resolution.
  private static let videoWidth: Int32 = 640
  private static let videoHeight: Int32 = 360

  private var videoSource: GStreamerVideoTexture?

  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    GeneratedPluginRegistrant.register(with: self)

    guard let controller = window?.rootViewController as? FlutterViewController else {
      fatalError("rootViewController is not a FlutterViewController")
    }

    // Channel and method names mirror the other runners and lib/main.dart.
    let channel = FlutterMethodChannel(
      name: "win_texture_poc", binaryMessenger: controller.binaryMessenger)
    channel.setMethodCallHandler { [weak self] call, result in
      guard let self = self else { return }
      switch call.method {
      case "createTexture":
        result(self.createTexture())
      default:
        result(FlutterMethodNotImplemented)
      }
    }

    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }

  // Registers a Flutter texture backed by a CVPixelBuffer and points the
  // GStreamer test pipeline at it. Returns the Flutter texture id consumed by
  // the Texture widget in lib/main.dart.
  private func createTexture() -> Int64 {
    let registrar = self.registrar(forPlugin: "GStreamerTextureSource")!
    let source = GStreamerVideoTexture(
      textureRegistry: registrar.textures(),
      width: AppDelegate.videoWidth,
      height: AppDelegate.videoHeight)
    videoSource = source
    return source.textureId
  }
}
