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
#include <string>
#include "../Engine/Core/RANMobileEngine.h"
#include "../UI/Login/LoginScreen.h"

static JavaVM* g_jvm = nullptr;
static jclass g_bridgeClass = nullptr;
static RANMobileEngine* g_engine = nullptr;
static void* g_appContext = nullptr;

struct AndroidAppContext {
    RANMobileEngine engine;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool hasWindow = false;
    bool keyboardVisible = false;
};

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

    EGLint w = 1280, h = 720;
    eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &w);
    eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &h);
    // Recreate renderer resources for this exact EGL surface. This also runs
    // on surface recreation after an APP_CMD_TERM_WINDOW / INIT_WINDOW cycle.
    ctx->engine.OnSurfaceCreated(w, h);
    ctx->hasWindow = true;
}

static void TermEGL(AndroidAppContext* ctx) {
    ctx->engine.OnSurfaceLost();
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

// Apply Android DisplayCutout safe-area insets (API 28+) through JNI. Missing
// classes, older APIs and JNI errors degrade to zero insets without crashing.
static void ApplyDisplayCutoutInsets(ANativeActivity* activity, AndroidAppContext* ctx) {
    if (!activity || !activity->vm || !ctx) return;
    JNIEnv* env = nullptr;
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;

    float insets[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    jclass actCls = env->GetObjectClass(activity->clazz);
    jmethodID getWin = env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;");
    if (getWin) {
        jobject win = env->CallObjectMethod(activity->clazz, getWin);
        if (win) {
            jclass winCls = env->GetObjectClass(win);
            jmethodID getDecor = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
            if (getDecor) {
                jobject decor = env->CallObjectMethod(win, getDecor);
                if (decor) {
                    jclass viewCls = env->GetObjectClass(decor);
                    jmethodID getRootInsets = env->GetMethodID(
                        viewCls, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
                    if (getRootInsets) {
                        jobject wi = env->CallObjectMethod(decor, getRootInsets);
                        if (wi) {
                            jclass wiCls = env->GetObjectClass(wi);
                            jmethodID getCutout = env->GetMethodID(
                                wiCls, "getDisplayCutout", "()Landroid/view/DisplayCutout;");
                            if (getCutout) {
                                jobject cutout = env->CallObjectMethod(wi, getCutout);
                                if (cutout) {
                                    jclass cutCls = env->GetObjectClass(cutout);
                                    const char* names[4] = {
                                        "getSafeInsetLeft", "getSafeInsetTop",
                                        "getSafeInsetRight", "getSafeInsetBottom"
                                    };
                                    for (int i = 0; i < 4; ++i) {
                                        jmethodID m = env->GetMethodID(cutCls, names[i], "()I");
                                        if (m) insets[i] = static_cast<float>(
                                            env->CallIntMethod(cutout, m));
                                    }
                                    env->DeleteLocalRef(cutout);
                                }
                                env->DeleteLocalRef(wi);
                            }
                            env->DeleteLocalRef(wiCls);
                        }
                    }
                    env->DeleteLocalRef(decor);
                }
            }
            env->DeleteLocalRef(win);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    ctx->engine.SetSafeAreaInsets(insets[0], insets[1], insets[2], insets[3]);
    activity->vm->DetachCurrentThread();
}

static void ReleaseAndroidInput();

static void HandleAppCmd(struct android_app* app, int32_t cmd) {
    auto* ctx = static_cast<AndroidAppContext*>(app->userData);
    if (!ctx) return;
    if (cmd == APP_CMD_INIT_WINDOW && app->window != nullptr) {
        InitEGL(ctx, app->window);
        ApplyDisplayCutoutInsets(app->activity, ctx);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        ReleaseAndroidInput();
        ctx->keyboardVisible = false;
        if (g_engine) g_engine->SetKeyboardVisible(false);
        TermEGL(ctx);
    } else if (cmd == APP_CMD_PAUSE) {
        ReleaseAndroidInput();
        ctx->keyboardVisible = false;
        ctx->engine.SetKeyboardVisible(false);
        ctx->engine.Pause();
    } else if (cmd == APP_CMD_RESUME) {
        ctx->engine.Resume();
    } else if (cmd == APP_CMD_CONFIG_CHANGED && ctx->hasWindow && ctx->display != EGL_NO_DISPLAY) {
        EGLint w = 0, h = 0;
        eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &w);
        eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &h);
        if (w > 0 && h > 0) ctx->engine.Resize(w, h);
        ApplyDisplayCutoutInsets(app->activity, ctx);
    }
}

static bool AttachJNI(JNIEnv** env, bool* attached) {
    if (!g_jvm) return false;
    const jint result = g_jvm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6);
    if (result == JNI_OK) {
        *attached = false;
        return *env != nullptr;
    }
    if (result == JNI_EDETACHED && g_jvm->AttachCurrentThread(env, nullptr) == JNI_OK && *env) {
        *attached = true;
        return true;
    }
    return false;
}

static void DetachJNI(JNIEnv* env, bool attached) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
    if (attached && g_jvm) g_jvm->DetachCurrentThread();
}

static void CallBridgeVoid(JNIEnv* env, const char* name, const char* signature) {
    if (!g_bridgeClass || !env) return;
    jmethodID method = env->GetStaticMethodID(g_bridgeClass, name, signature);
    if (method) env->CallStaticVoidMethod(g_bridgeClass, method);
}

// Requests that the Java InputConnection focuses the matching native field.
// Text is passed as a UTF-8 string; no per-character key synthesis is used.
static void FocusAndroidTextField(LoginScreen* login, LoginFocus focus) {
    if (!login) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (!AttachJNI(&env, &attached)) return;
    const bool password = focus == LoginFocus::Password;
    const std::string current = focus == LoginFocus::Username ? login->GetUsername()
                              : focus == LoginFocus::Password ? login->GetPassword()
                              : std::string();
    const int field = focus == LoginFocus::Username ? 1
                    : focus == LoginFocus::Password ? 2 : 0;
    if (g_bridgeClass) {
        jmethodID method = env->GetStaticMethodID(g_bridgeClass, "focusField",
                                                  "(ILjava/lang/String;Z)V");
        if (method) {
            jstring text = env->NewStringUTF(current.c_str());
            if (text) {
                env->CallStaticVoidMethod(g_bridgeClass, method, field, text,
                                          password ? JNI_TRUE : JNI_FALSE);
                env->DeleteLocalRef(text);
            }
        }
    }
    DetachJNI(env, attached);
}

static void BlurAndroidTextField() {
    JNIEnv* env = nullptr;
    bool attached = false;
    if (!AttachJNI(&env, &attached)) return;
    CallBridgeVoid(env, "blurField", "()V");
    DetachJNI(env, attached);
}

static void ReleaseAndroidInput() {
    JNIEnv* env = nullptr;
    bool attached = false;
    if (!AttachJNI(&env, &attached)) return;
    CallBridgeVoid(env, "releaseInput", "()V");
    DetachJNI(env, attached);
}

static std::string JStringToUtf8(JNIEnv* env, jstring value) {
    if (!env || !value) return std::string();
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return std::string();
    const std::string out(chars);
    env->ReleaseStringUTFChars(value, chars);
    return out;
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
            if (ctx->engine.GetScreenState() == EngineScreenState::Login && ctx->engine.GetLoginScreen()) {
                const LoginFocus focus = ctx->engine.GetLoginScreen()->GetFocus();
                if (focus == LoginFocus::Username || focus == LoginFocus::Password) {
                    FocusAndroidTextField(ctx->engine.GetLoginScreen(), focus);
                } else {
                    BlurAndroidTextField();
                }
            }
        } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
            // Deliver every active pointer, not just the pointer that caused
            // the event, so pinch and two-thumb UI interactions stay in sync.
            const size_t count = static_cast<size_t>(AMotionEvent_getPointerCount(event));
            for (size_t i = 0; i < count; ++i) {
                const int32_t moveId = AMotionEvent_getPointerId(event, i);
                ctx->engine.OnTouchMove(moveId,
                                        AMotionEvent_getX(event, i),
                                        AMotionEvent_getY(event, i));
            }
        } else if (masked == AMOTION_EVENT_ACTION_CANCEL) {
            ctx->engine.OnTouchCancel(pointerId, x, y);
        } else if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP) {
            ctx->engine.OnTouchUp(pointerId, x, y);
        }
        return 1;
    }
    // Key events are deliberately not mapped to characters here: the Java
    // InputConnection owns text, deletion and IME actions for both soft and
    // hardware keyboards.
    (void)type;
    return 0;
}

// Java -> native IME callbacks. Text always arrives as a UTF-8 string; no
// Unicode codepoint is reinterpreted as a hardware key event.
extern "C" JNIEXPORT void JNICALL
Java_com_famhawaii_ranmobile_RanTextInputBridge_nativeTextChanged(
        JNIEnv* env, jclass, jint field, jstring text) {
    if (!g_engine || !env) return;
    const LoginFocus focus = field == 1 ? LoginFocus::Username
                           : field == 2 ? LoginFocus::Password : LoginFocus::None;
    g_engine->OnIMETextChanged(focus, JStringToUtf8(env, text));
}

extern "C" JNIEXPORT void JNICALL
Java_com_famhawaii_ranmobile_RanTextInputBridge_nativeFocusChanged(
        JNIEnv*, jclass, jint field) {
    if (!g_engine) return;
    const LoginFocus focus = field == 1 ? LoginFocus::Username
                           : field == 2 ? LoginFocus::Password : LoginFocus::None;
    g_engine->OnIMEFocusChanged(focus);
}

extern "C" JNIEXPORT void JNICALL
Java_com_famhawaii_ranmobile_RanTextInputBridge_nativeImeActionDone(
        JNIEnv*, jclass) {
    if (g_engine) g_engine->OnIMEActionDone();
}

extern "C" JNIEXPORT void JNICALL
Java_com_famhawaii_ranmobile_RanTextInputBridge_nativeKeyboardVisible(
        JNIEnv*, jclass, jboolean visible) {
    AndroidAppContext* ctx = static_cast<AndroidAppContext*>(g_appContext);
    const bool isVisible = visible == JNI_TRUE;
    if (ctx) ctx->keyboardVisible = isVisible;
    if (g_engine) g_engine->SetKeyboardVisible(isVisible);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_jvm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !env) {
        return JNI_ERR;
    }
    jclass local = env->FindClass("com/famhawaii/ranmobile/RanTextInputBridge");
    if (local) {
        g_bridgeClass = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    // Absence of the bridge class degrades to no IME, not a launch failure.
    return JNI_VERSION_1_6;
}

extern "C" void android_main(struct android_app* state) {
    AndroidAppContext ctx;
    g_appContext = &ctx;
    g_engine = &ctx.engine;
    state->userData = &ctx;
    state->onAppCmd = HandleAppCmd;
    state->onInputEvent = HandleInputEvent;

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
        if (ctx.engine.GetLoginScreen() && ctx.engine.GetLoginScreen()->WantsExit()) {
            ANativeActivity_finish(state->activity);
            break;
        }
    }
    ReleaseAndroidInput();
    ctx.engine.Shutdown();
    TermEGL(&ctx);
    g_engine = nullptr;
    g_appContext = nullptr;
}
#endif
