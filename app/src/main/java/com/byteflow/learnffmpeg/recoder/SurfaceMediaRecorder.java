package com.byteflow.learnffmpeg.recoder;

import com.byteflow.learnffmpeg.render.MediaFBORender;

import java.io.IOException;

public class SurfaceMediaRecorder implements MediaEncoder.MediaEncoderListener {
    private static final String TAG = "MediaRecorder";
    private static final String MEDIA_EXT = ".mp4";
    private MuxerWrapper mMuxerWrapper;

    public SurfaceMediaRecorder(MediaFBORender fboRender) {
        try {
            mMuxerWrapper = new MuxerWrapper(MEDIA_EXT);
            new VideoMediaEncoder(mMuxerWrapper, this, fboRender);
            new AudioMediaEncoder(mMuxerWrapper, this);
            mMuxerWrapper.prepare();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void startRecording() {
        if(mMuxerWrapper != null) {
            mMuxerWrapper.startRecording();
        }
    }

    public void stopRecording() {
        if(mMuxerWrapper != null) {
            mMuxerWrapper.stopRecording();
            mMuxerWrapper = null;
        }
    }

    @Override
    public void onPrepared(MediaEncoder encoder) {

    }

    @Override
    public void onStopped(MediaEncoder encoder) {

    }
}
