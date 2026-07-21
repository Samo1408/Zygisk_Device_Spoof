plugins {
    id("com.android.library")
}

android {
    namespace = "com.samo.zygisk_device_spoof"
    compileSdk = 34
    ndkVersion = "27.0.12077973"
    defaultConfig {
        minSdk = 26
        externalNativeBuild {
            cmake {
                arguments("-DMODULE_NAME=zygisk_device_spoof")
                abiFilters("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
                cppFlags("-std=c++20 -fno-exceptions -fno-rtti")
            }
        }
    }
    externalNativeBuild { cmake { path("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    buildTypes {
        debug { externalNativeBuild { cmake { cppFlags.add("-DDEBUG") } } }
        release { externalNativeBuild { cmake { cppFlags.add("-O3 -DNDEBUG") } } }
    }
}
val zipDebug by tasks.registering(Zip::class) {
    dependsOn("assembleDebug")
    archiveFileName.set("zygisk_device_spoof_debug.zip")
    destinationDirectory.set(file("$projectDir/release"))
    from("$projectDir/template")
    from("${layout.buildDirectory}/intermediates/stripped_native_libs/debug/out/lib") { into("zygisk") }
}
val zipRelease by tasks.registering(Zip::class) {
    dependsOn("assembleRelease")
    archiveFileName.set("zygisk_device_spoof_release.zip")
    destinationDirectory.set(file("$projectDir/release"))
    from("$projectDir/template")
    from("${layout.buildDirectory}/intermediates/stripped_native_libs/release/out/lib") { into("zygisk") }
}
