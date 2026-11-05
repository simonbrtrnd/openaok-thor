package org.openaok.game;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;

/**
 * A second display, such as the AYN Thor's lower screen: a window of its own,
 * whose surface the engine draws into (src/hal/SecondScreen) and whose touches
 * it reads. The window never takes the focus, so the controller and the keys
 * stay with the game.
 */
class BottomScreen implements SurfaceHolder.Callback, View.OnTouchListener {
    private Presentation presentation;

    void show(Activity activity) {
        if (presentation != null) {
            return;
        }
        Display display = findSecondScreen(activity);
        if (display == null) {
            return;
        }
        presentation = new Presentation(activity, display);
        presentation.getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
        SurfaceView view = new SurfaceView(presentation.getContext());
        view.getHolder().addCallback(this);
        view.setOnTouchListener(this);
        presentation.setContentView(view);
        presentation.show();
    }

    void hide() {
        if (presentation != null) {
            presentation.dismiss();
            presentation = null;
        }
    }

    private static Display findSecondScreen(Activity activity) {
        DisplayManager manager = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        int gameDisplayId = activity.getWindowManager().getDefaultDisplay().getDisplayId();
        for (Display display : manager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            if (display.getDisplayId() != gameDisplayId) {
                return display;
            }
        }
        return null;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        nativeSetSurface(holder.getSurface(), width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeSetSurface(null, 0, 0);
    }

    @Override
    public boolean onTouch(View view, MotionEvent event) {
        nativeTouch(event.getActionMasked(), event.getX() / view.getWidth(), event.getY() / view.getHeight());
        return true;
    }

    private static native void nativeSetSurface(Surface surface, int width, int height);

    private static native void nativeTouch(int action, float x, float y);
}
