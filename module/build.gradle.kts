plugins {
    alias(libs.plugins.agp.app)
}

val moduleId: String by rootProject.extra
val moduleName: String by rootProject.extra
val verCode: Int by rootProject.extra
val verName: String by rootProject.extra
val commitHash: String by rootProject.extra
val abiList: List<String> by rootProject.extra

android {
    defaultConfig {
        ndk {
            abiFilters.addAll(abiList)
        }
        externalNativeBuild {
            cmake {
                cppFlags("-std=c++20", "-Wno-deprecated-declarations")
                arguments(
                    "-DANDROID_STL=c++_static",
                    "-DMODULE_NAME=$moduleId"
                )
            }
        }
    }
    buildTypes {
        debug {
            externalNativeBuild {
                cmake { cppFlags("-DDEBUG") }
            }
        }
        release { }
    }
    externalNativeBuild {
        cmake { path("src/main/cpp/CMakeLists.txt") }
    }
}

androidComponents.onVariants { variant ->
    afterEvaluate {
        val variantLowered = variant.name.lowercase()
        val variantCapped = variant.name.replaceFirstChar { it.uppercase() }
        val buildTypeLowered = variant.buildType?.lowercase()

        val moduleDir = layout.buildDirectory.file("outputs/module/$variantLowered")
        val zipFileName = "$moduleName-$verName-$verCode-$commitHash-$buildTypeLowered.zip".replace(' ', '-')

        val prepareModuleFilesTask = tasks.register<Sync>("prepareModuleFiles$variantCapped") {
            group = "module"
            dependsOn("assemble$variantCapped")
            into(moduleDir)
            from(rootProject.layout.projectDirectory.file("README.md"))
            from(layout.projectDirectory.file("template")) {
                exclude("module.prop")
            }
            from(layout.projectDirectory.file("template")) {
                include("module.prop")
                expand(
                    "moduleId" to moduleId,
                    "moduleName" to moduleName,
                    "versionName" to "$verName ($verCode-$commitHash-$variantLowered)",
                    "versionCode" to verCode
                )
            }
            from(layout.buildDirectory.file("intermediates/stripped_native_libs/$variantLowered/strip${variantCapped}DebugSymbols/out/lib")) {
                into("lib")
            }
        }

        val zipTask = tasks.register<Zip>("zip$variantCapped") {
            group = "module"
            dependsOn(prepareModuleFilesTask)
            archiveFileName.set(zipFileName)
            destinationDirectory.set(layout.projectDirectory.file("release").asFile)
            from(moduleDir)
        }
    }
}
