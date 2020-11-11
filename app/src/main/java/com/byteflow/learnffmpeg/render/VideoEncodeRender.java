package com.byteflow.learnffmpeg.render;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLES30;
import android.opengl.Matrix;
import android.util.Log;

import com.byteflow.learnffmpeg.R;
import com.byteflow.learnffmpeg.util.ShaderUtil;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public class VideoEncodeRender implements GLRender {
    private static final String TAG = "VideoEncodeRender";
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
    protected FloatBuffer vertexBuffer;
    protected FloatBuffer textureBuffer;
    private int program;
    private int vaoId;
    private int vboId;
    private int textureId;
    private Context context;
    private float[] matrix = new float[16];

    public VideoEncodeRender(Context context, int textureId) {
        this.context = context;
        this.textureId = textureId;

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
        program = ShaderUtil.createProgram(ShaderUtil.readRawTxt(context, R.raw.vertex_shader),
                ShaderUtil.readRawTxt(context, R.raw.fragment_shader));
        if (program > 0) {
            createVAO();
        }
    }

    @Override
    public void onSurfaceChanged(int width, int height) {
        GLES20.glViewport(0, 0, width, height);
    }

    @Override
    public void onDrawFrame() {
        Log.d(TAG, "onDrawFrame() called");
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        GLES20.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        GLES20.glUseProgram(program);
        GLES30.glBindVertexArray(vaoId);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glUniform1i(GLES20.glGetUniformLocation(program, "s_texture"), 0);

        resetMatrix();
        setAngle(180, 1.0f, 0.0f, 0.0f);//上下镜像
        GLES20.glUniformMatrix4fv(GLES20.glGetUniformLocation(program, "u_MVPMatrix"), 1, false, matrix, 0);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }

    @Override
    public void onSurfaceDestroy() {
        if (program > 0) {
            GLES20.glDeleteProgram(program);
            Log.d(TAG, "onSurfaceDestroy() called glDeleteProgram");
            program = 0;
        }

        if (vboId > 0) {
            GLES20.glDeleteBuffers(1, new int[]{vboId}, 0);
            vboId = 0;
            Log.d(TAG, "onSurfaceDestroy() called glDeleteBuffers");
        }

        if (vaoId > 0) {
            GLES30.glDeleteVertexArrays(1, new int[]{vaoId}, 0);
            vaoId = 0;
            Log.d(TAG, "onSurfaceDestroy() called glDeleteVertexArrays");
        }

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

    public void resetMatrix() {
        Matrix.setIdentityM(matrix, 0);
    }

    public void setAngle(float angle, float x, float y, float z) {
        Matrix.rotateM(matrix, 0, angle, x, y, z);
    }

}
