#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../Source/Capture.h"
#include <cassert>
#include <thread>
int main() {
    Capture capture; assert(capture.start(48000)); assert(!capture.start(48000));
    float left[480]={}, right[480]={};for(int i=0;i<480;++i){left[i]=0.4f;right[i]=0.2f;}
    const float* channels[]={left,right};
    std::thread producer([&] {while(capture.status()==Capture::recording) capture.push(channels,2,480);});
    std::vector<float> out;double rate=0;
    while(!capture.take(out,rate)) std::this_thread::yield();
    producer.join();assert(rate==24000);assert(out.size()==720000);
    assert(std::abs(out[100]-0.3f)<0.00001);
    assert(capture.start(44100));capture.requestStop();capture.push(channels,2,480);
    assert(capture.take(out,rate));assert(out.empty());assert(capture.status()==Capture::idle);
    assert(capture.start(48000));capture.push(channels,2,480);
    capture.finishAfterProducerStopped();assert(capture.take(out,rate));assert(out.size()==240);
    assert(capture.start(0)==false);
}
