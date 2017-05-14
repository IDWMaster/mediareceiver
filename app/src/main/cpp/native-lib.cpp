#include <jni.h>
#include <string>


extern "C" {



class VideoReceiver {
public:

    VideoReceiver() {

    }
};

static VideoReceiver instance;

JNIEXPORT void JNICALL
Java_com_idwnetcloudcomputing_mediareceiverfast_MainActivity_loadPacket(JNIEnv *env, jclass type,
                                                                        jobject buffy) {
    void* buffer = env->GetDirectBufferAddress(buffy);

}

}