# ANDROID_DEVICE_TEST_GUIDE.md — RAN ONLINE EP7 (FAM HAWAII MOBILE)

## 1. Install Debug APK
```bash
adb install -r app-debug.apk
```

## 2. Launch NativeActivity
```bash
adb shell am start -n com.famhawaii.ranmobile/android.app.NativeActivity
```

## 3. Monitor Logcat & Crash
```bash
adb logcat -s RANMobileEngine MobileRenderer AudioManager LoginScreen AndroidRuntime
```

## 4. Check Memory & Lifecycle
```bash
adb shell dumpsys meminfo com.famhawaii.ranmobile
```
