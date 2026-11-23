#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "../Engine/Core/RANMobileEngine.h"
#include "../Engine/Touch/TouchSystem.h"

struct AndroidAppContext {
    RANMobileEngine engine;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool hasWindow = false;
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

    EGLConfig config;
    EGLint numConfigs = 0;
    eglChooseConfig(ctx->display, attribs, &config, 1, &numConfigs);

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, ctxAttribs);
    ctx->surface = eglCreateWindowSurface(ctx->display, config, window, nullptr);
    eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);

    EGLint w = 1280, h = 720;
    eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &w);
    eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &h);
    ctx->engine.Initialize();
    ctx->engine.Start();
    ctx->hasWindow = true;
}

static void TermEGL(AndroidAppContext* ctx) {
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
        InitEGL(ctx, app->window);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        TermEGL(ctx);
    }
}

static int32_t HandleInputEvent(struct android_app* app, AInputEvent* event) {
    auto* ctx = static_cast<AndroidAppContext*>(app->userData);
    if (!ctx || !ctx->engine.IsInitialized()) return 0;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
        int32_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int32_t pointerId = AMotionEvent_getPointerId(event, idx);
        float x = AMotionEvent_getX(event, idx);
        float y = AMotionEvent_getY(event, idx);

        auto* touch = ctx->engine.GetTouchSystem();
        if (touch) {
            if (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                touch->OnTouchDown(pointerId, x, y);
            } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
                touch->OnTouchMove(pointerId, x, y);
            } else if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP) {
                touch->OnTouchUp(pointerId, x, y);
            }
        }
        return 1;
    }
    return 0;
}

extern "C" void android_main(struct android_app* state) {
    AndroidAppContext ctx;
    state->userData = &ctx;
    state->onAppCmd = HandleAppCmd;
    state->onInputEvent = HandleInputEvent;

    int events;
    struct android_poll_source* source;
    while (!state->destroyRequested) {
        while (ALooper_pollAll(ctx.hasWindow ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) {
                source->process(state, source);
            }
            if (state->destroyRequested) break;
        }
        if (ctx.hasWindow && ctx.engine.IsRunning()) {
            ctx.engine.Update(1.0f / 60.0f);
            ctx.engine.Render();
            eglSwapBuffers(ctx.display, ctx.surface);
        }
    }
    ctx.engine.Shutdown();
    TermEGL(&ctx);
}
#endif
