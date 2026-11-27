package com.famhawaii.ranmobile;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

/**
 * Java-side IME bridge. An almost invisible EditText owns the real Android
 * InputConnection; all committed text is forwarded to C++ as UTF-8 strings.
 * No game protocol, account data or login network behaviour lives here.
 */
public final class RanTextInputBridge {
    public static final int FIELD_NONE = 0;
    public static final int FIELD_USERNAME = 1;
    public static final int FIELD_PASSWORD = 2;

    private static RanTextInputBridge sInstance;

    private final Activity mActivity;
    private final EditText mEditor;
    private final InputMethodManager mImm;
    private final ViewTreeObserver.OnGlobalLayoutListener mLayoutListener;
    private volatile int mField = FIELD_NONE;
    private volatile boolean mApplyingNativeText = false;
    private volatile boolean mKeyboardVisible = false;
    private volatile boolean mDisposed = false;

    public RanTextInputBridge(Activity activity) {
        mActivity = activity;
        mImm = (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);

        mEditor = new EditText(activity);
        mEditor.setBackgroundColor(0x00000000);
        mEditor.setTextColor(0x00000000);
        mEditor.setCursorVisible(false);
        mEditor.setSingleLine(true);
        mEditor.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mEditor.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        }
        mEditor.setLayoutParams(new FrameLayout.LayoutParams(1, 1, Gravity.TOP | Gravity.START));
        activity.addContentView(mEditor, mEditor.getLayoutParams());

        mEditor.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) { }
            @Override public void afterTextChanged(Editable s) {
                if (mApplyingNativeText || mField == FIELD_NONE || mDisposed) return;
                nativeTextChanged(mField, s.toString());
            }
        });
        mEditor.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE ||
                actionId == EditorInfo.IME_ACTION_GO ||
                actionId == EditorInfo.IME_ACTION_SEARCH ||
                actionId == EditorInfo.IME_ACTION_SEND) {
                nativeImeActionDone();
                hideKeyboard(false);
                return true;
            }
            return false;
        });

        final View decor = activity.getWindow().getDecorView();
        mLayoutListener = () -> {
            if (mDisposed) return;
            final Rect frame = new Rect();
            decor.getWindowVisibleDisplayFrame(frame);
            final int height = Math.max(1, decor.getHeight());
            final boolean visible = (height - frame.height()) > height / 5;
            if (visible != mKeyboardVisible) {
                mKeyboardVisible = visible;
                nativeKeyboardVisible(visible);
            }
        };
        decor.getViewTreeObserver().addOnGlobalLayoutListener(mLayoutListener);
        sInstance = this;
    }

    /** Called from C++ when a native text field gains focus. */
    public static void focusField(int field, String currentText, boolean password) {
        final RanTextInputBridge bridge = sInstance;
        if (bridge != null) bridge.focus(field, currentText, password);
    }

    /** Called from C++ when focus leaves all text fields. */
    public static void blurField() {
        final RanTextInputBridge bridge = sInstance;
        if (bridge != null) bridge.blur();
    }

    /** Called from C++ during pause / surface loss. */
    public static void releaseInput() {
        final RanTextInputBridge bridge = sInstance;
        if (bridge != null) bridge.blur();
    }

    private void focus(int field, String currentText, boolean password) {
        if (mDisposed) return;
        final int safeField = (field == FIELD_USERNAME || field == FIELD_PASSWORD)
                ? field : FIELD_NONE;
        mField = safeField;
        mApplyingNativeText = true;
        final String safeText = currentText == null ? "" : currentText;
        mEditor.setText(safeText);
        mEditor.setSelection(mEditor.getText().length());
        mEditor.setInputType(password
                ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                : InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL);
        mApplyingNativeText = false;
        mEditor.requestFocus();
        if (mImm != null) {
            mImm.showSoftInput(mEditor, InputMethodManager.SHOW_IMPLICIT);
        }
        nativeFocusChanged(safeField);
    }

    private void blur() {
        if (mDisposed) return;
        mField = FIELD_NONE;
        mEditor.clearFocus();
        hideKeyboard(false);
        nativeFocusChanged(FIELD_NONE);
    }

    private void hideKeyboard(boolean clearFocus) {
        if (clearFocus) mEditor.clearFocus();
        if (mImm != null) {
            mImm.hideSoftInputFromWindow(mEditor.getWindowToken(), 0);
        }
    }

    public boolean handleBack() {
        if (mDisposed || mField == FIELD_NONE) return false;
        blur();
        return true;
    }

    public void dispose() {
        if (mDisposed) return;
        mDisposed = true;
        hideKeyboard(true);
        if (mKeyboardVisible) {
            mKeyboardVisible = false;
            nativeKeyboardVisible(false);
        }
        final View decor = mActivity.getWindow().getDecorView();
        decor.getViewTreeObserver().removeOnGlobalLayoutListener(mLayoutListener);
        if (sInstance == this) sInstance = null;
    }

    private static native void nativeTextChanged(int field, String text);
    private static native void nativeFocusChanged(int field);
    private static native void nativeImeActionDone();
    private static native void nativeKeyboardVisible(boolean visible);
}
