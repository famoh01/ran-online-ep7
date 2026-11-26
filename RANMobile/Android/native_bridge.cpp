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
#include "../Engine/Core/RANMobileEngine.h"
#include "../UI/Login/LoginScreen.h"

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

static void AppendUTF8(std::string& out, int32_t codepoint) {
    if (codepoint <= 0 || codepoint > 0x10FFFF) return;
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

static void ShowAndroidKeyboard(ANativeActivity* activity, bool show);

static void HandleAppCmd(struct android_app* app, int32_t cmd) {
    auto* ctx = static_cast<AndroidAppContext*>(app->userData);
    if (!ctx) return;
    if (cmd == APP_CMD_INIT_WINDOW && app->window != nullptr) {
        InitEGL(ctx, app->window);
        ApplyDisplayCutoutInsets(app->activity, ctx);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        ShowAndroidKeyboard(app->activity, false);
        ctx->keyboardVisible = false;
        TermEGL(ctx);
    } else if (cmd == APP_CMD_PAUSE) {
        ShowAndroidKeyboard(app->activity, false);
        ctx->keyboardVisible = false;
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
                const bool wantsKeyboard = focus == LoginFocus::Username ||
                                           focus == LoginFocus::Password;
                if (wantsKeyboard != ctx->keyboardVisible) {
                    ShowAndroidKeyboard(app->activity, wantsKeyboard);
                    ctx->keyboardVisible = wantsKeyboard;
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
        const int32_t unicode = AKeyEvent_getUnicodeChar(event, 0);
        if (unicode > 0x7F) {
            std::string utf8;
            AppendUTF8(utf8, unicode);
            if (!utf8.empty()) {
                ctx->engine.OnTextInput(utf8);
                return 1;
            }
        }
    }
    return 0;
}

extern "C" void android_main(struct android_app* state) {
    AndroidAppContext ctx;
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
    ctx.engine.Shutdown();
    TermEGL(&ctx);
}
#endif
