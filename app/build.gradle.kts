plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// O SDL3 não vai para o git: é descarregado por tools/fetch-deps.ps1.
val sdlDir = rootProject.file("third_party/SDL3")
if (!File(sdlDir, ".version").exists()) {
    throw GradleException(
        "Falta o SDL3. Na pasta do projeto, corre primeiro:  .\\tools\\fetch-deps.ps1"
    )
}

android {
    namespace = "pt.orimbux.pad"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "pt.orimbux.pad"
        // Só interessa o Galaxy S25 Ultra (Android 15+ = API 35).
        minSdk = 35
        targetSdk = 36
        versionCode = 3
        versionName = "0.3-raio-x"

        ndk {
            // O S25 Ultra é arm64. Compilar só esta arquitetura poupa tempo e espaço.
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared")
                cppFlags += listOf("-std=c++20")
            }
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        prefab = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.6"
        }
    }

    sourceSets {
        getByName("main") {
            // Classes Java do SDL3 (HIDDeviceManager, SDLControllerManager, ...).
            java.srcDir(File(sdlDir, "android-project/app/src/main/java"))
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.09.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.core:core-ktx:1.16.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.2")
    implementation("com.google.oboe:oboe:1.10.0")
}
