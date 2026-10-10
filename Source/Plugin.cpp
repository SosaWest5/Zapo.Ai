#include <JuceHeader.h>
#include <BinaryData.h>
#include "Capture.h"
#include <map>

class ZapoEditor;
class ZapoProcessor final : public juce::AudioProcessor {
public:
    Capture beat;
    ZapoProcessor() : AudioProcessor(BusesProperties().withInput("Beat",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)) {}
    const juce::String getName() const override { return "Zapo.Ai"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override { beat.finishAfterProducerStopped(); }
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainInputChannelSet()==l.getMainOutputChannelSet() &&
          (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
    }
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        beat.push(b.getArrayOfReadPointers(),b.getNumChannels(),b.getNumSamples());
        // Deliberate bit-for-bit audio passthrough; no effects, gain changes or latency.
    }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& b) override { b.setSize(0); }
    void setStateInformation(const void*,int) override {}
};

static juce::var object(std::initializer_list<std::pair<juce::String,juce::var>> values) {
    auto* o=new juce::DynamicObject(); for(auto& p:values) o->setProperty(p.first,p.second); return juce::var(o);
}

class TalkButton final : public juce::TextButton {
public:
    std::function<void()> begin, end;
    void mouseDown(const juce::MouseEvent& e) override { TextButton::mouseDown(e); if(begin) begin(); }
    void mouseUp(const juce::MouseEvent& e) override { TextButton::mouseUp(e); if(end) end(); }
};

class ZapoEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::AudioIODeviceCallback {
public:
    explicit ZapoEditor(ZapoProcessor& p):AudioProcessorEditor(p),processor(p) {
        mic.addAudioCallback(this);
        setSize(900,640); setResizable(true,true); setResizeLimits(760,580,1300,950);
        addAndMakeVisible(chat); chat.setMultiLine(true); chat.setReadOnly(true); chat.setScrollbarsShown(true);
        chat.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff171d26));
        chat.setColour(juce::TextEditor::textColourId,juce::Colour(0xffe8eef6));
        addAndMakeVisible(input); input.setTextToShowWhenEmpty("Ask Zapo about your beat, chords or music history...",juce::Colours::grey);
        input.onReturnKey=[this]{sendText("chat");};
        for(auto* b:std::initializer_list<juce::Button*>{&send,&listen,&talk,&micSetup,&help,&advice,&micToggle}) addAndMakeVisible(b);
        send.setButtonText("Send"); listen.setButtonText("Listen to Beat"); talk.setButtonText("Hold to Talk");
        micSetup.setButtonText("Mic settings"); help.setButtonText("Teach / Tips"); advice.setButtonText("Taking your advice"); micToggle.setButtonText("Toggle mic");
        micToggle.setClickingTogglesState(true);
        send.onClick=[this]{sendText("chat");}; help.onClick=[this]{sendText("tips");};
        advice.onClick=[this]{if(!busy && !capturing()) {setMood("coach"); append("Zapo","Coach mode. Try the suggestion in FL, then listen again. Tell me what you changed.");}};
        listen.onClick=[this]{
            if(processor.beat.status()==Capture::recording) {processor.beat.requestStop(); return;}
            if(busy || capturing()) return;
            if(!bridgeAvailable()) return;
            if(processor.getSampleRate()<=0) {append("Zapo","Start playback in FL Studio first.");return;}
            if(processor.beat.start(processor.getSampleRate())) {beatOwned=true;listen.setButtonText("Stop & Review"); elapsed=0; append("Zapo","Listening to the Master. Play your beat; press Stop & Review when ready (30 seconds maximum).");}
        };
        talk.begin=[this]{if(!micToggle.getToggleState()) startMic();};
        talk.end=[this]{if(!micToggle.getToggleState()) voice.requestStop();};
        micToggle.onClick=[this]{if(micToggle.getToggleState()) {if(!startMic()) micToggle.setToggleState(false,juce::dontSendNotification);} else voice.requestStop();};
        micSetup.onClick=[this]{showMicSettings();};
        struct Asset {const char* name;const char* data;int size;};
        const Asset assets[]={
            {"scared",BinaryData::scared_jpg,BinaryData::scared_jpgSize},{"disgusted",BinaryData::disgusted_jpg,BinaryData::disgusted_jpgSize},
            {"feeling",BinaryData::feeling_jpg,BinaryData::feeling_jpgSize},{"coach",BinaryData::coach_jpg,BinaryData::coach_jpgSize},
            {"unimpressed",BinaryData::unimpressed_jpg,BinaryData::unimpressed_jpgSize},{"teacher",BinaryData::teacher_jpg,BinaryData::teacher_jpgSize},
            {"stank",BinaryData::stank_jpg,BinaryData::stank_jpgSize},{"happy",BinaryData::happy_jpg,BinaryData::happy_jpgSize}};
        for(const auto& a:assets) faces[a.name]=juce::ImageCache::getFromMemory(a.data,a.size);
        setMood("happy"); append("Zapo","I'm Zapo. Ask me a question, or start FL playback and hit Listen to Beat. AI requests send the chosen clip or question to your configured service. Start the bridge first.");
        startTimerHz(10);
    }
    ~ZapoEditor() override {
        stopTimer(); processor.beat.requestStop();
        if(micDialog) delete micDialog.getComponent();
        mic.removeAudioCallback(this); mic.closeAudioDevice();
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff10151d));
        g.setColour(juce::Colour(0xffffa43b));g.setFont(30);g.drawText("Zapo.Ai",24,16,250,42,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff9aa9bb));g.setFont(14);g.drawText("YOUR PRODUCER SIDEKICK",26,57,300,25,juce::Justification::centredLeft);
        auto area=juce::Rectangle<float>(24,100,250,(float)getHeight()-270);
        g.setColour(juce::Colours::white);g.fillRoundedRectangle(area,16);
        auto im=faces[mood];g.drawImageWithin(im,(int)area.getX()+8,(int)area.getY()+8,(int)area.getWidth()-16,(int)area.getHeight()-16,juce::RectanglePlacement::centred);
        g.setColour(juce::Colour(0xffffa43b));g.setFont(16);g.drawText(mood.toUpperCase(),24,getHeight()-160,250,24,juce::Justification::centred);
        g.setColour(juce::Colour(0xff9aa9bb));g.setFont(13);
        auto state=busy?"Thinking...":capturing()?"Capturing audio · "+juce::String(elapsed,1)+"s":"Ready · replies in text";
        g.drawText(state,300,55,getWidth()-325,28,juce::Justification::centredLeft);
    }
    void resized() override {
        int w=getWidth(),h=getHeight();chat.setBounds(300,100,w-324,h-240);
        input.setBounds(300,h-126,w-412,36);send.setBounds(w-100,h-126,76,36);
        listen.setBounds(300,h-76,170,38);talk.setBounds(480,h-76,150,38);micToggle.setBounds(640,h-76,120,38);
        micSetup.setBounds(24,h-65,250,30);help.setBounds(24,h-128,250,28);advice.setBounds(24,h-96,250,28);
    }
private:
    ZapoProcessor& processor;
    juce::TextEditor chat,input;juce::TextButton send,listen,micSetup,help,advice;TalkButton talk;juce::ToggleButton micToggle;
    juce::Component::SafePointer<juce::DialogWindow> micDialog;
    juce::AudioDeviceManager mic;Capture voice;std::map<juce::String,juce::Image> faces;juce::String mood="happy";
    juce::Array<juce::var> history;bool busy=false,beatOwned=false;double elapsed=0;juce::String bridgeToken;
    bool capturing() const {return processor.beat.status()!=Capture::idle || voice.status()!=Capture::idle;}
    void setMood(const juce::String& m) {if(faces.count(m)) mood=m;repaint();}
    void append(const juce::String& who,const juce::String& text) {
        chat.moveCaretToEnd();chat.insertTextAtCaret(who+": "+text+"\n\n");
    }
    bool bridgeAvailable() {
        auto f=juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".zapo-ai/bridge.json");
        auto config=juce::JSON::parse(f.loadFileAsString());bridgeToken=config["token"].toString();
        if(bridgeToken.isEmpty()) {append("Zapo","Run bridge/start-windows.bat first. It connects me to the AI service.");return false;} return true;
    }
    bool startMic() {
        if(busy || capturing() || !bridgeAvailable()) return false;
        if(!mic.getCurrentAudioDevice()) mic.restartLastAudioDevice();
        if(!mic.getCurrentAudioDevice()) {
            auto error=mic.initialise(1,0,nullptr,true);
            if(error.isNotEmpty()) {append("Zapo","Microphone unavailable: "+error+". Open Mic settings and choose a Windows audio device.");return false;}
        }
        auto* d=mic.getCurrentAudioDevice();
        if(!d || d->getActiveInputChannels().countNumberOfSetBits()==0) {append("Zapo","Select a microphone in Mic settings.");return false;}
        bool started=voice.start(d->getCurrentSampleRate());
        if(started) {elapsed=0;append("You","Microphone on. Speak your question; release Hold to Talk or switch off Toggle mic to send.");}return started;
    }
    void showMicSettings() {
        if(capturing() || busy) return;
        if(micDialog) {micDialog->toFront(true);return;}
        if(!mic.getCurrentAudioDevice()) {mic.initialise(1,0,nullptr,true);}
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned(new juce::AudioDeviceSelectorComponent(mic,1,1,0,0,false,false,true,false));
        o.content->setSize(460,400);o.dialogTitle="Zapo microphone (separate from beat input)";
        o.dialogBackgroundColour=juce::Colour(0xff171d26);o.useNativeTitleBar=true;o.resizable=false;micDialog=o.launchAsync();
    }
    void audioDeviceIOCallbackWithContext(const float* const* in,int ni,float* const* out,int no,int n,const juce::AudioIODeviceCallbackContext&) override {
        voice.push(in,ni,n);for(int c=0;c<no;++c) if(out[c]) juce::FloatVectorOperations::clear(out[c],n);
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {voice.finishAfterProducerStopped();}
    void timerCallback() override {
        std::vector<float> samples;double rate=0;
        if(capturing()) elapsed+=0.1;
        if(processor.beat.take(samples,rate)) {listen.setButtonText("Listen to Beat");if(beatOwned) sendAudio("beat",samples,rate);beatOwned=false;}
        if(voice.take(samples,rate)) {micToggle.setToggleState(false,juce::dontSendNotification);mic.closeAudioDevice();sendAudio("voice",samples,rate);}
        if(voice.status()==Capture::idle && !micDialog && mic.getCurrentAudioDevice()) mic.closeAudioDevice();
        repaint();
    }
    void sendText(const juce::String& mode) {
        if(busy || capturing() || !bridgeAvailable()) return;
        auto text=input.getText().trim();if(text.isEmpty() && mode=="tips") text="Give me one useful beat-making tip and explain why it works.";
        if(text.isEmpty()) return;
        input.clear();append("You",text);if(mode=="tips") setMood("teacher");
        request(object({{"mode",mode},{"text",text},{"history",history}}),text);
    }
    void sendAudio(const juce::String& mode,const std::vector<float>& samples,double rate) {
        if(samples.size()<(size_t)(rate*0.3)) {append("Zapo","Clip too short. Capture at least a second and try again.");return;}
        double sum=0;float peak=0;int clipped=0;
        juce::MemoryBlock bytes;
        auto* stream=new juce::MemoryOutputStream(bytes,false);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream,rate,1,16,{},0));
        if(!writer) {delete stream;append("Zapo","Could not encode audio.");return;}
        juce::AudioBuffer<float> b(1,(int)samples.size());
        for(size_t i=0;i<samples.size();++i) {auto v=samples[i];if(!std::isfinite(v)) v=0;b.setSample(0,(int)i,v);sum+=(double)v*v;peak=std::max(peak,std::abs(v));if(std::abs(v)>=1)++clipped;}
        if(peak<0.00001f) {append("Zapo","I captured silence. Check playback or microphone routing and try again.");return;}
        writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples());writer.reset();
        auto data=juce::Base64::toBase64(bytes.getData(),bytes.getSize());
        auto metrics=object({{"duration",samples.size()/rate},{"rms_db",20*std::log10(std::max(1e-9,std::sqrt(sum/samples.size())))},
          {"peak_db",20*std::log10(std::max(1e-9,(double)peak))},{"clipped_fraction",(double)clipped/samples.size()}});
        auto text=input.getText().trim();input.clear();
        request(object({{"mode",mode},{"audio",data},{"text",text},{"metrics",metrics},{"history",history}}),mode=="beat"?"[Beat review] "+text:"[Spoken question]");
    }
    void request(juce::var payload,juce::String userText) {
        busy=true;repaint();auto token=bridgeToken;
        juce::Component::SafePointer<ZapoEditor> safe(this);
        juce::Thread::launch([safe,payload,token,userText] {
            int code=0;juce::String error;juce::var result;
            auto url=juce::URL("http://127.0.0.1:8765/ask").withPOSTData(juce::JSON::toString(payload));
            auto options=juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
              .withExtraHeaders("Content-Type: application/json\r\nAuthorization: Bearer "+token+"\r\n")
              .withConnectionTimeoutMs(120000).withStatusCode(&code).withNumRedirectsToFollow(0);
            auto stream=url.createInputStream(options);
            if(stream) result=juce::JSON::parse(stream->readEntireStreamAsString());
            if(!stream) error="Can't reach the Zapo bridge. Start bridge/start-windows.bat and try again.";
            else if(code!=200) error=result["error"].toString().isNotEmpty()?result["error"].toString():"AI service error ("+juce::String(code)+").";
            else if(result["reply"].toString().isEmpty()) error="AI returned an empty reply. Try again.";
            juce::MessageManager::callAsync([safe,result,error,userText] {
                if(!safe) return;
                auto& e=*safe;e.busy=false;
                if(error.isNotEmpty()) {e.append("Zapo",error);e.repaint();return;}
                auto reply=result["reply"].toString();e.append("Zapo",reply);e.setMood(result["mood"].toString());
                e.history.add(object({{"role","user"},{"content",userText}}));e.history.add(object({{"role","assistant"},{"content",reply}}));
                while(e.history.size()>12) e.history.remove(0);
            });
        });
    }
};
juce::AudioProcessorEditor* ZapoProcessor::createEditor() { return new ZapoEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new ZapoProcessor();}
