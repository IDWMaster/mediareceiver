#include <jni.h>
#include <string>
#include "cppext/cppext.h"
#include <thread>
#include <queue>
#include <condition_variable>

extern "C" {


class PacketInfo {
public:
    //Packet ID (short) -- Total length (int) -- Segment ID (short) -- Segment size (byte)
    uint16_t packetId;
    uint32_t length;
    size_t segSize;
    size_t avail;
    unsigned char* packet;
    PacketInfo() {
        packet = 0;
        avail = 0;
    }
    ~PacketInfo(){
        if(packet) {
            delete[] packet;
        }
    }
};

class Packet {
public:
    unsigned char* data;
    size_t len;
    Packet(unsigned char* data, size_t len) {
        this->data = data;
        this->len = len;
    }
    ~Packet() {
        delete[] data;
    }
};

class VideoReceiver {
public:
    std::shared_ptr<System::Net::UDPSocket> server;
    std::thread* thread;
    std::thread* assembler;
    PacketInfo packets[30];
    unsigned char mander[8192];
    size_t currentIdx = 0;
    std::queue<Packet*> assembled;
    std::condition_variable evt;
    std::mutex mtx;
    std::queue<Packet*> pendingAssembly;
    VideoReceiver() {
        assembler = new std::thread([=](){
            while(true) {
                std::unique_lock<std::mutex> l(mtx);
                while (pendingAssembly.empty()) {
                    evt.wait(l);
                }
                Packet *packet = pendingAssembly.front();
                pendingAssembly.pop();
                l.unlock();
                try {
                    System::BStream str(packet->data, packet->len);
                    uint16_t pid;
                    uint32_t tlen;
                    uint16_t sid;
                    unsigned char segsize;
                    str.Read(pid);
                    str.Read(tlen);
                    str.Read(sid);
                    str.Read(segsize);

                    PacketInfo *info = 0;
                    for (size_t i = 0; i < 30; i++) {
                        if (packets[i].packet) {
                            if (packets[i].packetId == pid) {
                                info = packets + i;
                                break;
                            }
                        }

                    }
                    if (!info) {
                        info = packets + currentIdx;
                        currentIdx = (currentIdx + 1) % 30;
                        info->avail = 0;
                        if (info->packet) {
                            delete[] info->packet;
                        }
                        info->packet = new unsigned char[tlen];
                        info->length = tlen;
                        info->packetId = pid;
                        info->segSize = 1 << (size_t) segsize;
                    }

                    System::BStream ostr(info->packet, info->length);
                    ostr.Increment(info->segSize * sid);
                    ostr.Write(str.ptr, str.length);
                    info->avail += str.length;
                    if (info->avail >= info->length) {
                        Packet *p = new Packet(info->packet, info->length);
                        std::unique_lock<std::mutex> l(mtx);
                        assembled.push(p);
                        info->packet = 0;
                        evt.notify_all();
                    }
                } catch (const char *err) {

                }
                delete packet;
            }
        });
        thread = new std::thread([=](){
            System::Net::IPEndpoint ep;
            ep.ip = "::";
            ep.port = 9090;
            server = System::Net::CreateUDPSocket(ep);
            std::shared_ptr<std::shared_ptr<System::Net::UDPCallback>> cb = std::make_shared<std::shared_ptr<System::Net::UDPCallback>>();
            *cb = System::Net::F2UDPCB([=](const System::Net::UDPCallback& data){
                auto start = std::chrono::steady_clock::now();
                unsigned char* raw = new unsigned char[data.outlen];
                memcpy(raw,mander,data.outlen);
                Packet* p = new Packet(raw,data.outlen);
                std::unique_lock<std::mutex> l(mtx);
                pendingAssembly.push(p);
                evt.notify_all();
                l.unlock();
                server->Receive(mander,8192,*cb);
                auto count = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
                if(count>10) {
                    printf("TOO unsigned long long!");
                }

            });

            server->Receive(mander,8192,*cb);
            System::Enter();
        });

    }
    Packet* getPacket() {
        std::unique_lock<std::mutex> l(mtx);
        while(assembled.empty()) {
            evt.wait(l);
        }
        Packet* packet = assembled.front();
        assembled.pop();
        return packet;

    }
    ~VideoReceiver(){

    }
};

static VideoReceiver instance;

JNIEXPORT jint JNICALL
Java_com_idwnetcloudcomputing_mediareceiverfast_MainActivity_loadPacket(JNIEnv *env, jclass type,
                                                                        jobject buffy) {
    void* buffer = env->GetDirectBufferAddress(buffy);
    Packet* packet = instance.getPacket();
    if(packet->len<=env->GetDirectBufferCapacity(buffy)) {
        memcpy(buffer, packet->data, packet->len);
        int plen = packet->len;
        delete packet;
        return plen;
    }
    delete packet;
    return 0;
}

}