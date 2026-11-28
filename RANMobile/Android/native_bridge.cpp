#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include "../Engine/Core/RANMobileEngine.h"
#include "../Engine/Renderer/GpuSelfTest.h"
#include "../UI/Login/LoginScreen.h"

struct AndroidAppContext {
    RANMobileEngine engine;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool hasWindow = false;
};

// B19: launch-mode flag.  When the activity is started with the intent extra
// "selftest" = "gpu", the app runs the live-GLES3 verification harness once
// (after the EGL surface exists), writes gpu_selftest.json + a PPM screenshot
// into the app's internal files dir, and exits.  The normal game path is
// untouched when the extra is absent.
static std::string g_selfTestMode;
static bool g_selfTestDone = false;

static std::string GetSelfTestMode(ANativeActivity* activity) {
    JNIEnv* env = nullptr;
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return "";
    jclass actCls = env->GetObjectClass(activity->clazz);
    jmethodID getIntent = env->GetMethodID(actCls, "getIntent", "()Landroid/content/Intent;");
    std::string mode;
    jobject intent = getIntent ? env->CallObjectMethod(activity->clazz, getIntent) : nullptr;
    if (intent) {
        jclass intentCls = env->GetObjectClass(intent);
        jmethodID getStr =
            env->GetMethodID(intentCls, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
        jstring key = env->NewStringUTF("selftest");
        jstring value =
            getStr ? static_cast<jstring>(env->CallObjectMethod(intent, getStr, key)) : nullptr;
        if (value) {
            const char* c = env->GetStringUTFChars(value, nullptr);
            if (c) {
                mode = c;
                env->ReleaseStringUTFChars(value, c);
            }
            env->DeleteLocalRef(value);
        }
        env->DeleteLocalRef(key);
    }
    activity->vm->DetachCurrentThread();
    return mode;
}

static ANativeActivity* ctx_activity = nullptr;   // B19 selftest target (valid during init)
static std::string g_authAssetRoot;               // B19: optional adb-push root (unused on device path)

static void InitEGL(AndroidAppContext* ctx, ANativeWindow* window) {
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(ctx->display, nullptr, nullptr);

    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(ctx->display, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        const EGLint fallbackAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
        };
        eglChooseConfig(ctx->display, fallbackAttribs, &config, 1, &numConfigs);
    }
    if (numConfigs == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "RANMobile", "eglChooseConfig failed: no matching config");
        return;
    }

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (ctx->context == EGL_NO_CONTEXT) {
        const EGLint es2Attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, es2Attribs);
    }
    ctx->surface = eglCreateWindowSurface(ctx->display, config, window, nullptr);
    if (ctx->context == EGL_NO_CONTEXT || ctx->surface == EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_ERROR, "RANMobile", "eglCreateContext/Surface failed");
        return;
    }
    eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);
    // B19 CI: bracket logs — the +8s window showed a run stuck between
    // APP_CMD_INIT_WINDOW and "EGL+engine ready" with nothing in between;
    // these lines pinpoint the exact stage (EGL calls vs engine Initialize).
    __android_log_print(ANDROID_LOG_INFO, "RANMobile", "EGL context+surface current");

    EGLint w = 1280, h = 720;
    eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &w);
    eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &h);
    if (!ctx->engine.IsInitialized()) {
        if (!ctx->engine.Initialize()) {
            __android_log_print(ANDROID_LOG_ERROR, "RANMobile",
                                "engine Initialize FAILED (renderer init returned false)");
        }
    } else {
        // B19: re-entry after context loss (rotation/restore) — rebuild GL objects.
        ctx->engine.EnsureGpuContext(w, h);
    }
    ctx->engine.Resize(w, h);
    ctx->hasWindow = true;
    __android_log_print(ANDROID_LOG_INFO, "RANMobile", "EGL+engine ready %dx%d, selftest=%s",
                        w, h, g_selfTestMode.empty() ? "<none>" : g_selfTestMode.c_str());

    // B19 GPU verification mode: run the harness once, persist the verdict, exit.
    if (g_selfTestMode == "gpu" && !g_selfTestDone) {
        g_selfTestDone = true;
        ANativeActivity* activity = ctx_activity;
        const char* dataPath = activity->internalDataPath;
        const std::string outDir = (dataPath && *dataPath) ? dataPath : "/data/local/tmp";
        // The files dir is created lazily by the framework; make sure it exists
        // before the harness writes its verdict (best effort).
        mkdir(outDir.c_str(), 0775);
        std::string shot = outDir + "/gpu_selftest.ppm";
        const RANMobile::GpuSelfTestResult result = RANMobile::RunGpuSelfTest(
            *ctx->engine.GetRenderer(), activity->assetManager, g_authAssetRoot, shot);
        const std::string jsonPath = outDir + "/gpu_selftest.json";
        std::FILE* f = std::fopen(jsonPath.c_str(), "w");
        if (f) {
            std::fwrite(result.json.data(), 1, result.json.size(), f);
            std::fclose(f);
        }
        __android_log_print(ANDROID_LOG_INFO, "RANMobile", "gpu selftest verdict=%s -> %s",
                            result.overallPass ? "PASS" : "FAIL", jsonPath.c_str());
        // B19 CI: logcat is the only retrieval channel that worked on every CI
        // run (run-as cat returns empty; adbd-root cat died mid-poll once).
        // Emit the verdict as COMPACT single-line chunks under a fixed tag;
        // CI reassembles the chunks by index.  Chunk < 3000 chars stays well
        // under the ~4068-char logcat line limit.
        std::string compact;
        compact.reserve(result.json.size() + 16);
        for (char c : result.json) {
            if (c == '\n' || c == '\t' || c == '\r') compact += ' ';
            else compact += c;
        }
        constexpr size_t kChunk = 3000;
        const size_t parts = (compact.size() + kChunk - 1) / kChunk;
        for (size_t i = 0; i < parts; ++i) {
            __android_log_print(ANDROID_LOG_INFO, "GPUSELFTEST_JSON", "%zu/%zu %s",
                                i + 1, parts, compact.substr(i * kChunk, kChunk).c_str());
        }
        ANativeActivity_finish(activity);
        return;
    }
    ctx->engine.Start();
}

static void TermEGL(AndroidAppContext* ctx) {
    // B19: the EGL context is about to be destroyed — release every GL object the
    // engine owns while the context is still current, so the next window rebuilds
    // instead of drawing with stale handles (rotation / restore / background).
    if (ctx->hasWindow) {
        ctx->engine.OnGpuContextLost();
    }
    if (ctx->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (ctx->context != EGL_NO_CONTEXT) eglDestroyContext(ctx->display, ctx->context);
        if (ctx->surface != EGL_NO_SURFACE) eglDestroySurface(ctx->display, ctx->surface);
        eglTerminate(ctx->display);
    }
    ctx->display = EGL_NO_DISPLAY;
    ctx->context = EGL_NO_CONTEXT;
    ctx->surface = EGL_NO_SURFACE;
    ctx->hasWindow = false;
}

static void HandleAppCmd(struct android_app* app, int32_t cmd) {
    auto* ctx = static_cast<AndroidAppContext*>(app->userData);
    if (!ctx) return;
    if (cmd == APP_CMD_INIT_WINDOW && app->window != nullptr) {
        __android_log_print(ANDROID_LOG_INFO, "RANMobile", "APP_CMD_INIT_WINDOW -> InitEGL");
        InitEGL(ctx, app->window);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        __android_log_print(ANDROID_LOG_INFO, "RANMobile", "APP_CMD_TERM_WINDOW -> TermEGL");
        TermEGL(ctx);
    } else if (cmd == APP_CMD_PAUSE) {
        ctx->engine.Pause();
    } else if (cmd == APP_CMD_RESUME) {
        ctx->engine.Resume();
    }
}

static void ShowAndroidKeyboard(ANativeActivity* activity, bool show) {
    if (!activity || !activity->vm) return;
    JNIEnv* env = nullptr;
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;

    jclass actCls = env->GetObjectClass(activity->clazz);
    jmethodID getSysSvc = env->GetMethodID(actCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring immStr = env->NewStringUTF("input_method");
    jobject imm = env->CallObjectMethod(activity->clazz, getSysSvc, immStr);
    if (imm) {
        jclass immCls = env->GetObjectClass(imm);
        jmethodID getWin = env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;");
        jobject win = env->CallObjectMethod(activity->clazz, getWin);
        if (win) {
            jclass winCls = env->GetObjectClass(win);
            jmethodID getDecor = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
            jobject decor = env->CallObjectMethod(win, getDecor);
            if (decor) {
                if (show) {
                    jmethodID toggleSoft = env->GetMethodID(immCls, "toggleSoftInput", "(II)V");
                    if (toggleSoft) {
                        env->CallVoidMethod(imm, toggleSoft, 2, 0); // SHOW_FORCED
                    }
                } else {
                    jclass viewCls = env->GetObjectClass(decor);
                    jmethodID getTok = env->GetMethodID(viewCls, "getWindowToken", "()Landroid/os/IBinder;");
                    jobject tok = env->CallObjectMethod(decor, getTok);
                    jmethodID hideSoft = env->GetMethodID(immCls, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
                    env->CallBooleanMethod(imm, hideSoft, tok, 0);
                }
            }
        }
    }
    env->DeleteLocalRef(immStr);
    activity->vm->DetachCurrentThread();
}

static int32_t HandleInputEvent(struct android_app* app, AInputEvent* event) {
    auto* ctx = static_cast<AndroidAppContext*>(app->userData);
    if (!ctx || !ctx->engine.IsInitialized()) return 0;

    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
        int32_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int32_t pointerId = AMotionEvent_getPointerId(event, idx);
        float x = AMotionEvent_getX(event, idx);
        float y = AMotionEvent_getY(event, idx);

        if (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            ctx->engine.OnTouchDown(pointerId, x, y);
            if (ctx->engine.GetLoginScreen()) {
                auto focus = ctx->engine.GetLoginScreen()->GetFocus();
                if (focus == LoginFocus::Username || focus == LoginFocus::Password) {
                    ShowAndroidKeyboard(app->activity, true);
                } else {
                    ShowAndroidKeyboard(app->activity, false);
                }
            }
        } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
            ctx->engine.OnTouchMove(pointerId, x, y);
        } else if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP || masked == AMOTION_EVENT_ACTION_CANCEL) {
            ctx->engine.OnTouchUp(pointerId, x, y);
        }
        return 1;
    } else if (type == AINPUT_EVENT_TYPE_KEY && AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN) {
        int32_t keyCode = AKeyEvent_getKeyCode(event);
        if (keyCode == AKEYCODE_DEL) {
            ctx->engine.OnBackspace();
            return 1;
        } else if (keyCode == AKEYCODE_ENTER && ctx->engine.GetLoginScreen()) {
            ctx->engine.GetLoginScreen()->OnLoginPressed();
            return 1;
        } else if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z) {
            char c = static_cast<char>('a' + (keyCode - AKEYCODE_A));
            ctx->engine.OnTextInput(std::string(1, c));
            return 1;
        } else if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9) {
            char c = static_cast<char>('0' + (keyCode - AKEYCODE_0));
            ctx->engine.OnTextInput(std::string(1, c));
            return 1;
        } else if (keyCode == AKEYCODE_PERIOD) {
            ctx->engine.OnTextInput(".");
            return 1;
        } else if (keyCode == AKEYCODE_MINUS) {
            ctx->engine.OnTextInput("-");
            return 1;
        } else if (keyCode == AKEYCODE_AT) {
            ctx->engine.OnTextInput("@");
            return 1;
        } else if (keyCode == AKEYCODE_SPACE) {
            ctx->engine.OnTextInput(" ");
            return 1;
        }
    }
    return 0;
}

extern "C" void android_main(struct android_app* state) {
    AndroidAppContext ctx;
    state->userData = &ctx;
    state->onAppCmd = HandleAppCmd;
    state->onInputEvent = HandleInputEvent;
    ctx_activity = state->activity;
    g_selfTestMode = GetSelfTestMode(state->activity);
    // Unconditional entry log: if this line is absent from logcat, the native
    // library never ran (activity start / library load failure upstream).
    __android_log_print(ANDROID_LOG_INFO, "RANMobile", "android_main entered, selftest=%s",
                        g_selfTestMode.empty() ? "<none>" : g_selfTestMode.c_str());

    auto lastTime = std::chrono::steady_clock::now();
    int events;
    struct android_poll_source* source;
    while (!state->destroyRequested) {
        while (ALooper_pollAll(ctx.hasWindow ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) {
                source->process(state, source);
            }
            if (state->destroyRequested) break;
        }
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        if (ctx.hasWindow && ctx.engine.IsRunning()) {
            ctx.engine.Update(dt);
            ctx.engine.Render();
            eglSwapBuffers(ctx.display, ctx.surface);
        }
        if (g_selfTestDone) break;  // B19: selftest writes its verdict and exits
        if (ctx.engine.GetLoginScreen() && ctx.engine.GetLoginScreen()->WantsExit()) {
            ANativeActivity_finish(state->activity);
            break;
        }
    }
    ctx.engine.Shutdown();
    TermEGL(&ctx);
}
#endif
