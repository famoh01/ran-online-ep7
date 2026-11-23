# BUILD_ANDROID.md — Building RANMobile.apk / RANMobile.aab

## Prerequisites
- Android NDK r25+
- Android SDK (API 24+ / targetSdk 34)
- CMake 3.22+
- Gradle 8.x

## Build Native Library (ARM64)
```bash
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24
cmake --build build-android -j
```

## Build APK / AAB
Open `RANMobile/Android/` in Android Studio or run:
```bash
./gradlew assembleRelease   # Outputs RANMobile.apk
./gradlew bundleRelease     # Outputs RANMobile.aab
```
