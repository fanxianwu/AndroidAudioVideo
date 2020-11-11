package com.byteflow.learnffmpeg.recoder;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.opengl.EGLContext;
import android.util.Log;
import android.util.Size;
import android.view.Surface;

import com.byteflow.learnffmpeg.render.GLRender;
import com.byteflow.learnffmpeg.render.MediaFBORender;
import com.byteflow.learnffmpeg.render.VideoEncodeRender;
import com.byteflow.learnffmpeg.util.EGLBase;

import java.io.IOException;

public class VideoMediaEncoder extends MediaEncoder implements MediaFBORender.OnFrameAvailableListener {
	private static final boolean DEBUG = false;	// TODO set false on release
	private static final String TAG = "MediaVideoEncoder";

	private static final String MIME_TYPE = "video/avc";
	// parameters for recording
    private static final int FRAME_RATE = 25;
    private static final float BPP = 0.25f;
    private final int mWidth;
    private final int mHeight;
    private Surface mSurface;
    private EncoderEGLThread mEGLThread;

	public VideoMediaEncoder(final MuxerWrapper muxer, final MediaEncoderListener listener, MediaFBORender mediaFBORender) {
		super(muxer, listener);
		if (DEBUG) Log.i(TAG, "MediaVideoEncoder: ");
		Size viewportSize = mediaFBORender.getViewportSize();
		mWidth = viewportSize.getWidth();
		mHeight = viewportSize.getHeight();
		mEGLThread = new EncoderEGLThread(
				mediaFBORender.getEglContext(),
				new VideoEncodeRender(mediaFBORender.getContext(), mediaFBORender.getTextureId()),
				this);
		mediaFBORender.setOnFrameAvailableListener(this);
	}

	@Override
	public boolean frameAvailableSoon() {
		boolean result;
		if (result = super.frameAvailableSoon() && mEGLThread != null)
			mEGLThread.requestRender();
		return result;
	}

	@Override
	protected void prepare() throws IOException {
		if (DEBUG) Log.i(TAG, "prepare: ");
        mTrackIndex = -1;
        mMuxerStarted = mIsEOS = false;

        final MediaCodecInfo videoCodecInfo = selectVideoCodec(MIME_TYPE);
        if (videoCodecInfo == null) {
            Log.e(TAG, "Unable to find an appropriate codec for " + MIME_TYPE);
            return;
        }
		if (DEBUG) Log.i(TAG, "selected codec: " + videoCodecInfo.getName());

        final MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE, mWidth, mHeight);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);	// API >= 18
        format.setInteger(MediaFormat.KEY_BIT_RATE, calcBitRate());
        format.setInteger(MediaFormat.KEY_FRAME_RATE, FRAME_RATE);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 10);
		if (DEBUG) Log.i(TAG, "format: " + format);

        mMediaCodec = MediaCodec.createEncoderByType(MIME_TYPE);
        mMediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        // get Surface for encoder input
        // this method only can call between #configure and #start
        mSurface = mMediaCodec.createInputSurface();	// API >= 18
        mMediaCodec.start();
		mEGLThread.start();
        if (DEBUG) Log.i(TAG, "prepare finishing");
        if (mListener != null) {
        	try {
        		mListener.onPrepared(this);
        	} catch (final Exception e) {
        		Log.e(TAG, "prepare:", e);
        	}
        }
	}

	@Override
    protected void release() {
		if (DEBUG) Log.i(TAG, "release:");
		if (mSurface != null) {
			mSurface.release();
			mSurface = null;
		}

		if (mEGLThread != null) {
			mEGLThread.exit();
			try {
				mEGLThread.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			mEGLThread = null;
		}
		super.release();
	}

	private int calcBitRate() {
		final int bitrate = (int)(BPP * FRAME_RATE * mWidth * mHeight);
		Log.i(TAG, String.format("bitrate=%5.2f[Mbps]", bitrate / 1024f / 1024f));
		return bitrate;
	}

    /**
     * select the first codec that match a specific MIME type
     * @param mimeType
     * @return null if no codec matched
     */
    protected static final MediaCodecInfo selectVideoCodec(final String mimeType) {
    	if (DEBUG) Log.v(TAG, "selectVideoCodec:");

    	// get the list of available codecs
        final int numCodecs = MediaCodecList.getCodecCount();
        for (int i = 0; i < numCodecs; i++) {
        	final MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);

            if (!codecInfo.isEncoder()) {	// skipp decoder
                continue;
            }
            // select first codec that match a specific MIME type and color format
            final String[] types = codecInfo.getSupportedTypes();
            for (int j = 0; j < types.length; j++) {
                if (types[j].equalsIgnoreCase(mimeType)) {
                	if (DEBUG) Log.i(TAG, "codec:" + codecInfo.getName() + ",MIME=" + types[j]);
            		final int format = selectColorFormat(codecInfo, mimeType);
                	if (format > 0) {
                		return codecInfo;
                	}
                }
            }
        }
        return null;
    }

    /**
     * select color format available on specific codec and we can use.
     * @return 0 if no colorFormat is matched
     */
    protected static final int selectColorFormat(final MediaCodecInfo codecInfo, final String mimeType) {
		if (DEBUG) Log.i(TAG, "selectColorFormat: ");
    	int result = 0;
    	final MediaCodecInfo.CodecCapabilities caps;
    	try {
    		Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
    		caps = codecInfo.getCapabilitiesForType(mimeType);
    	} finally {
    		Thread.currentThread().setPriority(Thread.NORM_PRIORITY);
    	}
        int colorFormat;
        for (int i = 0; i < caps.colorFormats.length; i++) {
        	colorFormat = caps.colorFormats[i];
            if (isRecognizedViewoFormat(colorFormat)) {
            	if (result == 0)
            		result = colorFormat;
                break;
            }
        }
        if (result == 0)
        	Log.e(TAG, "couldn't find a good color format for " + codecInfo.getName() + " / " + mimeType);
        return result;
    }

	/**
	 * color formats that we can use in this class
	 */
    protected static int[] recognizedFormats;
	static {
		recognizedFormats = new int[] {
//        	MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar,
//        	MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
//        	MediaCodecInfo.CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
        	MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface,
		};
	}

    private static final boolean isRecognizedViewoFormat(final int colorFormat) {
		if (DEBUG) Log.i(TAG, "isRecognizedViewoFormat:colorFormat=" + colorFormat);
    	final int n = recognizedFormats != null ? recognizedFormats.length : 0;
    	for (int i = 0; i < n; i++) {
    		if (recognizedFormats[i] == colorFormat) {
    			return true;
    		}
    	}
    	return false;
    }

    @Override
    protected void signalEndOfInputStream() {
		if (DEBUG) Log.d(TAG, "sending EOS to encoder");
		mMediaCodec.signalEndOfInputStream();	// API >= 18
		mIsEOS = true;
	}

	@Override
	public void onFrameAvailable() {
		frameAvailableSoon();
	}

	static class EncoderEGLThread extends Thread {
		private static final String TAG = "EncoderEGLThread";
		private VideoMediaEncoder mVideoEncoder;
		private GLRender mVideoEncoderRender;
		private EGLBase mEgl;
		private EGLBase.EglSurface mInputSurface;
		private EGLContext mSharedContext;
		private final Object mLock = new Object();
		private volatile boolean mIsExit = false;
		private volatile int mRequestDraw;

		public EncoderEGLThread(EGLContext context, GLRender render, VideoMediaEncoder videoMediaEncoder) {
			mVideoEncoder = videoMediaEncoder;
			mSharedContext = context;
			mVideoEncoderRender = render;
		}

		@Override
		public void run() {
			super.run();
			initEgl();

			if (mVideoEncoderRender != null) {
				mVideoEncoderRender.onSurfaceCreated();
				mVideoEncoderRender.onSurfaceChanged(mVideoEncoder.mWidth, mVideoEncoder.mHeight);
			}

			boolean localRequestDraw;
			while (true) {
				synchronized (mLock) {
					localRequestDraw = mRequestDraw > 0;
					if (localRequestDraw) {
						mRequestDraw--;
					}
				}

				if (localRequestDraw) {
					if (mEgl != null) {
						Log.d(TAG, "mVideoEncoderRender.onDrawFrame called");
						mInputSurface.makeCurrent();
						mVideoEncoderRender.onDrawFrame();
						mInputSurface.swap();
						mVideoEncoder.frameAvailableSoon();
					}
				} else {
					synchronized(mLock) {
						try {
							mLock.wait(10);
						} catch (final InterruptedException e) {
							break;
						}
					}
				}

				if(mIsExit) break;
			}

			if (mVideoEncoderRender != null) {
				mVideoEncoderRender.onSurfaceDestroy();
			}
			unInitEgl();
		}

		public void exit()
		{
			mIsExit = true;
			synchronized (mLock) {
				mLock.notifyAll();
			}
		}

		public void requestRender() {
			synchronized (mLock) {
				mRequestDraw++;
				mLock.notifyAll();
			}
		}

		private void initEgl() {
			mEgl = new EGLBase(mSharedContext, false, true);
			mInputSurface = mEgl.createFromSurface(mVideoEncoder.mSurface);
			mInputSurface.makeCurrent();
		}

		private void unInitEgl() {
			if (mInputSurface != null) {
				mInputSurface.release();
				mInputSurface = null;
			}

			if (mEgl != null) {
				mEgl.release();
				mEgl = null;
			}

			mVideoEncoder = null;

		}

	}

}
