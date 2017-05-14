package com.idwnetcloudcomputing.mediareceiverfast;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.support.v7.widget.LinearLayoutCompat;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

import java.io.IOException;
import java.nio.ByteBuffer;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        SurfaceView view = new SurfaceView(this);
        setContentView(view);
        SurfaceHolder eric = view.getHolder();
        eric.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                try {
                    final MediaCodec codec = MediaCodec.createDecoderByType("video/avc");
                    codec.configure(MediaFormat.createVideoFormat("video/avc",1280,720),holder.getSurface(),null,0);
                    codec.setOutputSurface(holder.getSurface());

                    codec.start();
                    Thread netThread = new Thread(new Runnable() {
                        @Override
                        public void run() {
                            while(true) {
                                int bufferId = codec.dequeueInputBuffer(-1);
                                ByteBuffer buffy = codec.getInputBuffer(bufferId);
                                int len = loadPacket(buffy);
                                codec.queueInputBuffer(bufferId,0,len,0,0);
                                MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                                int idx = codec.dequeueOutputBuffer(info,0);
                                if(idx>=0) {
                                    codec.releaseOutputBuffer(idx,true);
                                }
                            }
                        }
                    });
                    netThread.start();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {

            }
        });
    }
    public static native int loadPacket(ByteBuffer buffy);
}
