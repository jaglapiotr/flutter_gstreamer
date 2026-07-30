import java.util.Properties

plugins {
    id("com.android.application")
    id("kotlin-android")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

// Location of the unpacked universal GStreamer Android binaries. Shared by
// the CMake build (for headers/link-time symbols) and by the jniLibs staging
// task below (to physically bundle libgstreamer_android.so into the APK).
//
// gstAndroidRoot is machine-specific, so it lives in android/local.properties.
// Gradle does NOT expose local.properties keys as project properties
// automatically (only AGP/Flutter read a few well-known keys like sdk.dir /
// flutter.sdk), so the file is loaded explicitly here. Resolution order:
//   1. -PgstAndroidRoot=... or gstAndroidRoot in gradle.properties
//   2. gstAndroidRoot in android/local.properties
//   3. GSTREAMER_ROOT_ANDROID environment variable
val localProperties = Properties().apply {
    val localPropertiesFile = rootProject.file("local.properties")
    if (localPropertiesFile.exists()) {
        localPropertiesFile.inputStream().use { load(it) }
    }
}

val gstRoot: String = (
    (project.findProperty("gstAndroidRoot") as String?)
        ?: localProperties.getProperty("gstAndroidRoot")
        ?: System.getenv("GSTREAMER_ROOT_ANDROID")
) ?: throw GradleException("GSTREAMER_ROOT_ANDROID must be set (environment variable), or \"gstAndroidRoot\" must be defined in android/local.properties (or passed via -PgstAndroidRoot), pointing to the top level directory of the unpacked universal GStreamer Android binaries")

// Maps Android's ABI folder naming (used by jniLibs/APK packaging) to the
// per-arch folder naming under $GSTREAMER_ROOT_ANDROID. The prebuilt
// libgstreamer_android.so lives under folders named after the Android ABI
// itself (e.g. $GSTREAMER_ROOT_ANDROID/arm64-v8a/lib), so this is an
// identity mapping.
val gstArchForAbi = mapOf(
    "armeabi-v7a" to "armeabi-v7a",
    "arm64-v8a" to "arm64-v8a",
    "x86" to "x86",
    "x86_64" to "x86_64"
)

val gstreamerJniLibsDir = layout.buildDirectory.dir("gstreamerJniLibs")

// GStreamer's prebuilt libgstreamer_android.so is only linked at build time
// via CMake's IMPORTED target (see src/main/cpp/CMakeLists.txt); AGP does not
// automatically bundle IMPORTED libraries into the APK. This task copies
// each ABI's .so into Android's expected jniLibs/<abi>/ layout so it actually
// ships alongside gstreamer_android_texture.so and System.loadLibrary can
// find it at runtime.
val stageGstreamerNativeLibs = tasks.register<Copy>("stageGstreamerNativeLibs") {
    for ((abi, gstArch) in gstArchForAbi) {
        from("$gstRoot/$gstArch/lib") {
            include("libgstreamer_android.so")
            into(abi)
        }
    }
    into(gstreamerJniLibsDir)
}

android {
    namespace = "com.example.gstreamer_demo"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_17.toString()
    }

    sourceSets {
        getByName("main") {
            jniLibs.srcDir(gstreamerJniLibsDir)
        }
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "com.example.gstreamer_demo"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName

        externalNativeBuild {
            cmake {
                arguments(
                    "-DGSTREAMER_ROOT_ANDROID=$gstRoot",
                    "-DANDROID_STL=c++_shared"
                )

                targets("gstreamer_android_texture")

                // All archs except MIPS and MIPS64 are supported
                abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }


    buildTypes {
        release {
            // TODO: Add your own signing config for the release build.
            // Signing with the debug keys for now, so `flutter run --release` works.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

// Ensure libgstreamer_android.so is staged into jniLibs/<abi>/ before AGP
// merges native libraries for packaging into the APK.
tasks.matching { it.name.matches(Regex("merge.*JniLibFolders")) }.configureEach {
    dependsOn(stageGstreamerNativeLibs)
}

flutter {
    source = "../.."
}
