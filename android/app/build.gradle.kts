plugins {
    id("com.android.application")
    id("kotlin-android")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

// Location of the unpacked universal GStreamer Android binaries. Shared by
// the CMake build (for headers/link-time symbols) and by the jniLibs staging
// task below (to physically bundle libgstreamer_android.so into the APK).
val gstRoot: String = if (project.hasProperty("gstAndroidRoot")) {
    project.property("gstAndroidRoot") as String
} else {
    System.getenv("GSTREAMER_ROOT_ANDROID")
} ?: throw GradleException("GSTREAMER_ROOT_ANDROID must be set, or \"gstAndroidRoot\" must be defined in your gradle.properties in the top level directory of the unpacked universal GStreamer Android binaries")

// Maps Android's ABI folder naming (used by jniLibs/APK packaging) to
// GStreamer's own per-arch folder naming under $GSTREAMER_ROOT_ANDROID.
val gstArchForAbi = mapOf(
    "armeabi-v7a" to "armv7",
    "arm64-v8a" to "arm64",
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
