package com.famhawaii.ranmobile;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.WindowManager;

/**
 * NativeActivity host that keeps the GLES surface intact while an invisible
 * EditText provides a real Android InputConnection to the native engine.
 */
public final class RanNativeActivity extends NativeActivity {
    private RanTextInputBridge mTextInput;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mTextInput = new RanTextInputBridge(this);
        getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE |
                WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN);
    }

    @Override
    public void onBackPressed() {
        // First back press dismisses the keyboard / drops IME focus; the native
        // UI keeps handling back when no text field is active.
        if (mTextInput != null && mTextInput.handleBack()) {
            return;
        }
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        if (mTextInput != null) {
            mTextInput.dispose();
            mTextInput = null;
        }
        super.onDestroy();
    }
}
