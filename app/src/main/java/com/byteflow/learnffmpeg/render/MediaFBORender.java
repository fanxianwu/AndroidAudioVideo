package com.byteflow.learnffmpeg.render;

import android.content.Context;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.GLES20;
import android.opengl.GLES30;
import android.opengl.Matrix;
import android.util.Log;
import android.util.Size;

import com.byteflow.learnffmpeg.R;
import com.byteflow.learnffmpeg.util.ShaderUtil;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import static android.opengl.GLES20.GL_FRAMEBUFFER_BINDING;

public class MediaFBORender implements GLRender {
    private static final String TAG = "MediaFBORender";
    private static float VERTEX_COORD[] = {
            -1f, -1f, 0.0f,
             1f, -1f, 0.0f,
            -1f,  1f, 0.0f,
             1f,  1f, 0.0f,
    };
    private static float TEXTURE_COORD[] = {
            0f, 1f, 0.0f,
            1f, 1f, 0.0f,
            0f, 0f, 0.0f,
            1f, 0f, 0.0f,
    };
    private static final int COORDS_PER_VERTEX = 3;
    private static final int COORD_STRIDE = COORDS_PER_VERTEX * 4; // 4 bytes per vertex
    private FloatBuffer vertexBuffer;
    private FloatBuffer textureBuffer;
    private int program;
    private int fboId;
    private int fboTextureId;
    private int vboId;
    private int vaoId;
    private int defaultFBO;
    private float[] matrix = new float[16];
    private int viewportW, viewportH;
    private Context context;
    private OnFrameAvailableListener onFrameAvailableListener;
    private EGLContext eglContext;

    public MediaFBORender(Context context) {
        this.context = context;
        vertexBuffer = ByteBuffer.allocateDirect(VERTEX_COORD.length * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .put(VERTEX_COORD);
        vertexBuffer.position(0);

        textureBuffer = ByteBuffer.allocateDirect(TEXTURE_COORD.length * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .put(TEXTURE_COORD);
        textureBuffer.position(0);
    }

    @Override
    public void onSurfaceCreated() {
        Log.d(TAG, "onSurfaceCreated() called");
        program = ShaderUtil.createProgram(ShaderUtil.readRawTxt(context, R.raw.vertex_shader),
                ShaderUtil.readRawTxt(context, R.raw.fragment_shader));
        if (program > 0) {
            createVAO();
            eglContext = EGL14.eglGetCurrentContext();
        }
    }

    @Override
    public void onSurfaceChanged(int width, int height) {
        Log.d(TAG, "onSurfaceChanged() called with: width = [" + width + "], height = [" + height + "]");
        GLES20.glViewport(0, 0, width, height);
        if (viewportW != width || viewportH != height) {
            viewportW = width;
            viewportH = height;
            createFBO(viewportW, viewportH);
        }
    }

    public void onPreDrawFrame() {
        int [] fbos = new int[1];
        GLES30.glGetIntegerv(GL_FRAMEBUFFER_BINDING, fbos, 0);
        defaultFBO = fbos[0];
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, fboId);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        GLES20.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    @Override
    public void onDrawFrame() {
        Log.d(TAG, "onDrawFrame() called");
        if(onFrameAvailableListener != null)
            onFrameAvailableListener.onFrameAvailable();

        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, defaultFBO);
        GLES20.glUseProgram(program);
        GLES30.glBindVertexArray(vaoId);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, fboTextureId);
        GLES20.glUniform1i(GLES20.glGetUniformLocation(program, "s_texture"), 0);

        resetMatrix();
        setAngle(180, 1.0f, 0.0f, 0.0f);//上下镜像
        GLES20.glUniformMatrix4fv(GLES20.glGetUniformLocation(program, "u_MVPMatrix"), 1, false, matrix, 0);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    }

    @Override
    public void onSurfaceDestroy() {

    }

    public void setOnFrameAvailableListener(OnFrameAvailableListener listener) {
        onFrameAvailableListener = listener;
    }

    public Size getViewportSize() {
        return new Size(viewportW, viewportH);
    }

    public Context getContext() {
        return context;
    }

    public EGLContext getEglContext() {
        return eglContext;
    }

    public int getTextureId() {
        return fboTextureId;
    }

    public void resetMatrix() {
        Matrix.setIdentityM(matrix, 0);
    }

    public void setAngle(float angle, float x, float y, float z) {
        Matrix.rotateM(matrix, 0, angle, x, y, z);
    }

    private void createVAO() {
        int[] vbos = new int[1];
        GLES20.glGenBuffers(vbos.length, vbos, 0);
        vboId = vbos[0];
        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, vboId);
        GLES20.glBufferData(GLES20.GL_ARRAY_BUFFER, VERTEX_COORD.length * 4 + TEXTURE_COORD.length * 4, null, GLES20.GL_STATIC_DRAW);
        GLES20.glBufferSubData(GLES20.GL_ARRAY_BUFFER, 0, VERTEX_COORD.length * 4, vertexBuffer);
        GLES20.glBufferSubData(GLES20.GL_ARRAY_BUFFER, VERTEX_COORD.length * 4, TEXTURE_COORD.length * 4, textureBuffer);
        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);

        int[] vaos = new int[1];
        GLES30.glGenVertexArrays(vaos.length, vaos, 0);
        vaoId = vaos[0];
        GLES30.glBindVertexArray(vaoId);
        GLES30.glBindBuffer(GLES20.GL_ARRAY_BUFFER, vboId);
        GLES30.glEnableVertexAttribArray(0);
        GLES30.glVertexAttribPointer(0, COORDS_PER_VERTEX, GLES20.GL_FLOAT, false, COORD_STRIDE, 0);
        GLES30.glEnableVertexAttribArray(1);
        GLES30.glVertexAttribPointer(1, COORDS_PER_VERTEX, GLES20.GL_FLOAT, false, COORD_STRIDE, VERTEX_COORD.length * 4);
        GLES30.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);
        GLES30.glBindVertexArray(0);
    }

    private void createFBO(int w, int h) {
        if (fboTextureId > 0) {
            GLES20.glDeleteTextures(1, new int[]{fboTextureId}, 0);

            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, fboId);
            int[] textureIds = new int[1];
            GLES20.glGenTextures(1, textureIds, 0);
            fboTextureId = textureIds[0];
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, fboTextureId);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
            GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
                    GLES20.GL_TEXTURE_2D, fboTextureId, 0);

            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, w, h,
                    0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);

            if (GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER)
                    != GLES20.GL_FRAMEBUFFER_COMPLETE) {
                Log.e(TAG, "glFramebufferTexture2D error");
            }
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        } else {
            int[] fbos = new int[1];
            GLES20.glGenFramebuffers(1, fbos, 0);
            fboId = fbos[0];
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, fboId);
            int[] textureIds = new int[1];
            GLES20.glGenTextures(1, textureIds, 0);
            fboTextureId = textureIds[0];
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, fboTextureId);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
            GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
                    GLES20.GL_TEXTURE_2D, fboTextureId, 0);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, w, h,
                    0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);

            if (GLES20.glCheckFramebufferStatus(GLES20.GL_FRAMEBUFFER)
                    != GLES20.GL_FRAMEBUFFER_COMPLETE) {
                Log.e(TAG, "glFramebufferTexture2D error");
            }
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        }
    }

    public interface OnFrameAvailableListener {
        void onFrameAvailable();
    }

}
