#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include "../Engine/Core/RANMobileEngine.h"

void android_main(struct android_app* state) {
    RANMobileEngine engine;
    if (!engine.Initialize()) {
        return;
    }
    engine.Start();

    int events;
    struct android_poll_source* source;
    while (!state->destroyRequested) {
        while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) {
                source->process(state, source);
            }
        }
        engine.Update(1.0f / 60.0f);
        engine.Render();
    }
    engine.Shutdown();
}
#endif
