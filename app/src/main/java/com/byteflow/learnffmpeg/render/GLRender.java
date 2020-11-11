package com.byteflow.learnffmpeg.render;

public interface GLRender {

    void onSurfaceCreated();

    void onSurfaceChanged(int width, int height);

    void onDrawFrame();

    void onSurfaceDestroy();
}
