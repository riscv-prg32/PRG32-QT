package org.riscvprg32.prg32qt.tv;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import org.qtproject.qt.android.QtActivityBase;

public class Prg32TvActivity extends QtActivityBase {
    private int prg32KeyCode(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A:
                return KeyEvent.KEYCODE_Z;
            case KeyEvent.KEYCODE_BUTTON_B:
                return KeyEvent.KEYCODE_X;
            case KeyEvent.KEYCODE_BUTTON_START:
            case KeyEvent.KEYCODE_BUTTON_SELECT:
            case KeyEvent.KEYCODE_DPAD_CENTER:
                return KeyEvent.KEYCODE_ENTER;
            default:
                return keyCode;
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int mappedKeyCode = prg32KeyCode(event.getKeyCode());
        if (mappedKeyCode == event.getKeyCode()) {
            return super.dispatchKeyEvent(event);
        }
        KeyEvent mappedEvent = new KeyEvent(
                event.getDownTime(),
                event.getEventTime(),
                event.getAction(),
                mappedKeyCode,
                event.getRepeatCount(),
                event.getMetaState(),
                event.getDeviceId(),
                event.getScanCode(),
                event.getFlags(),
                event.getSource());
        return super.dispatchKeyEvent(mappedEvent);
    }

    private void enterImmersiveMode() {
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        enterImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterImmersiveMode();
        }
    }
}
