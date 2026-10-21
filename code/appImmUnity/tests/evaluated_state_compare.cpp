// CPU-only native evaluator comparison. No renderer, sound engine or production ABI.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "libImmImporter/src/fromImmersive/fromImmersive.h"
#include "libImmPlayer/src/mngrPlayer.h"
#include "libImmImporter/src/document/layerSound.h"
using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
// Sound scheduling metadata only: no encoded resource, decoder, sound renderer
// or audio engine is created. GetStartTime observes actual retrigger decisions;
// ForceRestart alone cannot do so because the loop path calls SetPlaying.
static void checkSoundRepeatScheduling(piLog* log) {
    auto run=[&](uint32_t repeats,bool nested,bool infiniteOuter) {
        Sequence sequence;
        require(sequence.Init(Sequence::Type::Animated,0,vec3(0.0f)),"sound scheduling sequence init");
        sequence.SetInitialSpawnArea(nullptr);
        Layer* root=sequence.GetRoot();
        require(root->Init(Layer::Type::Group,L"Root",true,trans3d::identity(),trans3d::identity(),1.0f,true,
            piTick(nested?5000:525),nested?(infiniteOuter?0:1):repeats,0xffffffff,log),"sound scheduling root init");
        Layer::AnimValue visible{}; visible.mBool=true;
        root->AddKey(piTick(0),Layer::AnimProperty::Visibility,visible,Layer::InterpolationType::None);
        Layer* parent=root;
        if(nested) {
            parent=sequence.CreateLayer(root);
            require(parent && parent->Init(Layer::Type::Group,L"Nested",true,trans3d::identity(),trans3d::identity(),1.0f,true,
                piTick(525),repeats,0xffffffff,log),"sound scheduling nested init");
            parent->AddKey(piTick(0),Layer::AnimProperty::Visibility,visible,Layer::InterpolationType::None);
        }
        Layer* layer=sequence.CreateLayer(parent);
        require(layer && layer->Init(Layer::Type::Sound,L"Sound",true,trans3d::identity(),trans3d::identity(),1.0f,false,
            piTick(0),0,0xffffffff,log),"sound scheduling layer init");
        auto* sound=new LayerSound();
        LayerSound::AttenuationParameters attenuation{};
        LayerSound::ModifierParameters modifier{};
        require(sound->Init(false,false,1.0f,attenuation,modifier,LayerSound::Type::Flat,10.0f,log),"sound scheduling sound init");
        layer->SetImplementation(sound);
        layer->AddKey(piTick(0),Layer::AnimProperty::Visibility,visible,Layer::InterpolationType::None);
        MngrPlayer player; require(player.Init(log),"sound scheduling player init");
        const int64_t epoch=1000000;
        player.Enter(&sequence,piTick(epoch));
        for(int64_t tick : {0,1,524,525,526,1049,1050,1051,1100,1575,2000,3000}) {
            sound->SetForceRestart(false); // simulate consumption of the initial restart request
            player.Update(piTick(epoch+tick));
            const int64_t cycle=tick/525;
            const int64_t activeCycle=repeats==0?cycle:std::min(cycle,int64_t(repeats)-1);
            const int64_t expectedStart=epoch+activeCycle*525;
            if(piTick::CastInt(layer->GetStartTime())!=expectedStart || sound->GetForceRestart()) {
                std::fprintf(stderr,"[IMM_EVALUATED_COMPARE] sound-repeat repeats=%u nested=%d outer-infinite=%d tick=%lld start=%lld expected=%lld forceRestart=%d\n",
                    repeats,nested,infiniteOuter,tick,piTick::CastInt(layer->GetStartTime()),expectedStart,sound->GetForceRestart());
                player.Exit(); player.Deinit(); sequence.Deinit(log);
                throw std::runtime_error("sound repeat scheduling metadata regression");
            }
        }
        player.Exit(); player.Deinit(); sequence.Deinit(log);
    };
    for(uint32_t repeats : {0u,1u,2u}) run(repeats,false,false);
    run(2,true,false);
    run(2,true,true);
    std::printf("[IMM_EVALUATED_COMPARE] sound-repeat scheduling passed root infinite/single/finite and nested finite under single/infinite root; no audible playback evaluated\n");
}
struct MetadataCollector : IStrokeCollector {
    void OnBeginLayer(uint32_t,uint32_t,const wchar_t*,bool,float) override {}
    void OnBeginDrawing(uint32_t) override {}
    void OnStroke(uint32_t,uint8_t,uint8_t,uint32_t,const Point*,const bound3&) override {}
    void OnEndDrawing() override {}
    void OnEndLayer() override {}
};
struct Input {
    Sequence sequence;
    MngrPlayer player;
    std::map<std::wstring, Layer*> layers;
    piLog* log;
    unsigned excluded = 0;
    Input(const wchar_t* path, piLog* logger) : log(logger) {
        piFile file;
        require(file.Open(path, L"rb"), "paint metadata file open failed");
        piTArray<uint8_t> bytes; require(bytes.Init(file.GetLength(),false),"input allocation failed");
        struct BytesCleanup { piTArray<uint8_t>& data; ~BytesCleanup(){data.End();} } bytesCleanup{bytes};
        require(file.Read(bytes.GetAddress(0),bytes.GetMaxLength())==bytes.GetMaxLength(),"input read failed");
        file.Close();
        MetadataCollector collector;
        require(ImportMetadataFromMemory(&bytes,&sequence,log,Drawing::ColorSpace::Gamma,
            Drawing::PaintRenderingTechnique::Static,&collector),"native staged metadata import failed");
        bool ok = true;
        sequence.Recurse([&](Layer* layer, int, int, bool) {
            const auto type = layer->GetType();
            if (type == Layer::Type::Group || type == Layer::Type::Paint) {
                // The staged importer populated frame maps and drawing descriptors
                // without decoding geometry or loading non-paint image/audio assets.
                if (type == Layer::Type::Paint) {
                    auto* paint=static_cast<LayerPaint*>(layer->GetImplementation());
                    for(unsigned frame=0;frame<paint->GetNumFrames();++frame)
                        if(paint->GetFrameBuffer()[frame]>=paint->GetNumDrawings()) {
                            std::fwprintf(stderr,L"[IMM_EVALUATED_COMPARE] invalid-frame %ls frame=%u drawing=%u drawings=%u\n",layer->GetFullName()->GetS(),frame,paint->GetFrameBuffer()[frame],paint->GetNumDrawings());
                            ok=false; break;
                        }
                }
                if (!layers.emplace(layer->GetFullName()->GetS(), layer).second) {
                    std::fwprintf(stderr,L"[IMM_EVALUATED_COMPARE] duplicate path %ls\n",layer->GetFullName()->GetS()); ok=false;
                }
            } else ++excluded;
            return true;
        }, false, false, false, false);
        require(ok, "paint metadata failed or duplicate full layer path");
        require(player.Init(log), "player init failed");
        player.Enter(&sequence, piTick(1000000));
    }
    ~Input() { player.Exit(); player.Deinit(); sequence.Deinit(log); }
};
static bool near(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) && std::abs(a-b) <= 1e-5 * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}
static bool transformEqual(const trans3d& a, const trans3d& b) {
    bool q = true, minusQ = true;
    const double av[] = {a.mRotation.x,a.mRotation.y,a.mRotation.z,a.mRotation.w};
    const double bv[] = {b.mRotation.x,b.mRotation.y,b.mRotation.z,b.mRotation.w};
    for(int i=0;i<4;++i) { q &= near(av[i],bv[i]); minusQ &= near(av[i],-bv[i]); }
    return (q || minusQ) && a.mFlip == b.mFlip && near(a.mScale,b.mScale) &&
        near(a.mTranslation.x,b.mTranslation.x) && near(a.mTranslation.y,b.mTranslation.y) && near(a.mTranslation.z,b.mTranslation.z);
}
static int activeDrawing(LayerPaint* paint) {
    if (!paint->GetNumFrames()) return -1;
    const Drawing* current = paint->GetCurrentDrawing();
    for(unsigned i=0;i<paint->GetNumDrawings();++i) if(paint->GetDrawing(i)==current) return static_cast<int>(i);
    throw std::runtime_error("active drawing is not in paint drawing table");
}
struct State {
    Layer::Type type;
    bool visible, worldVisible;
    double opacity,worldOpacity;
    trans3d local,world;
    unsigned frames=0,rate=0,repeats=0;
    int drawing=-1;
    bool playing=false;
};
using Snapshot=std::map<std::wstring,State>;
static Snapshot snapshot(Input& input) {
    Snapshot result;
    for(const auto& entry:input.layers) {
        Layer* layer=entry.second;
        State state{layer->GetType(),layer->GetVisible(),layer->GetWorldVisible(),layer->GetOpacity(),layer->GetWorldOpacity(),layer->GetTransform(),layer->GetTransformToWorld()};
        if(state.type==Layer::Type::Paint) {
            auto* paint=static_cast<LayerPaint*>(layer->GetImplementation());
            state.frames=paint->GetNumFrames(); state.rate=paint->GetFrameRate(); state.repeats=paint->GetMaxRepeatCount();
            state.playing=paint->GetPlaying(); state.drawing=activeDrawing(paint);
        }
        result.emplace(entry.first,state);
    }
    return result;
}
static unsigned diagnostics=0;
static unsigned compare(const Snapshot& a, const Snapshot& b, const char* mode, int64_t tick, bool comparePlaying=true) {
    unsigned errors=0;
    require(a.size()==b.size(), "supported layer path count differs");
    for(const auto& entry : a) {
        auto found=b.find(entry.first);
        require(found!=b.end(), "supported layer path missing from candidate");
        const State& x=entry.second; const State& y=found->second;
        const char* problem=nullptr;
        if(x.type!=y.type) problem="type";
        else if(x.visible!=y.visible || x.worldVisible!=y.worldVisible) problem="visibility";
        else if(!near(x.opacity,y.opacity) || !near(x.worldOpacity,y.worldOpacity)) problem="opacity";
        else if(!transformEqual(x.local,y.local)) problem="local-transform";
        else if(!transformEqual(x.world,y.world)) problem="world-transform";
        else if(x.frames!=y.frames || x.rate!=y.rate || x.repeats!=y.repeats || (comparePlaying && x.playing!=y.playing) ||
            ((comparePlaying || x.worldVisible || y.worldVisible) && x.drawing!=y.drawing)) problem="paint-playback";
        if(problem) { if(diagnostics++<12) std::fwprintf(stderr,L"[IMM_EVALUATED_COMPARE] %hs tick=%lld %ls %hs opacity=%.9g/%.9g worldOpacity=%.9g/%.9g drawing=%d/%d playing=%d/%d\n",mode,tick,entry.first.c_str(),problem,x.opacity,y.opacity,x.worldOpacity,y.worldOpacity,x.drawing,y.drawing,x.playing,y.playing); ++errors; }
    }
    return errors;
}
int wmain(int argc, wchar_t** argv) {
    try {
        require(argc==5 && std::wstring(argv[4])==L"paint-groups-root", "usage: reference.imm candidate.imm comma-ticks paint-groups-root");
        std::vector<int64_t> ticks; std::wstring list=argv[3]; size_t begin=0;
        while(begin<list.size()) { size_t end=list.find(L',',begin); auto part=list.substr(begin,end-begin); size_t used=0;
            int64_t value=std::stoll(part,&used); require(used==part.size() && value>=0 && value<INT64_MAX-10000000,"invalid tick");
            ticks.push_back(value); if(end==std::wstring::npos) break; begin=end+1; }
        require(!ticks.empty(),"empty tick list"); std::sort(ticks.begin(),ticks.end());
        piLog log; require(log.Init(nullptr,0),"log init");
        checkSoundRepeatScheduling(&log);
        std::fprintf(stderr,"[IMM_EVALUATED_COMPARE] loading reference\n");
        Input a(argv[1],&log);
        std::fprintf(stderr,"[IMM_EVALUATED_COMPARE] loading candidate\n");
        Input b(argv[2],&log); unsigned pairErrors=0,historyErrors=0;
        std::fprintf(stderr,"[IMM_EVALUATED_COMPARE] evaluating\n");
        // Sequential updates retain native history; explicit seeks and restart use
        // the actual MngrPlayer entry points, not hand-evaluated animation formulas.
        std::map<int64_t,Snapshot> baselineA,baselineB;
        struct Clock {piTick start,stop;};
        std::map<int64_t,Clock> clocksA,clocksB;
        for(auto tick:ticks) { a.player.Update(piTick(1000000+tick)); b.player.Update(piTick(1000000+tick));
            a.player.GetTime(piTick(1000000+tick),&clocksA[tick].start,&clocksA[tick].stop);
            b.player.GetTime(piTick(1000000+tick),&clocksB[tick].start,&clocksB[tick].stop);
            baselineA[tick]=snapshot(a); baselineB[tick]=snapshot(b); pairErrors+=compare(baselineA[tick],baselineB[tick],"sequential",tick); }
        int64_t now=2000000+ticks.back();
        a.player.Pause(piTick(now)); b.player.Pause(piTick(now));
        for(auto it=ticks.rbegin();it!=ticks.rend();++it) { ++now;
            a.player.SetTime(piTick(now),clocksA[*it].start,clocksA[*it].stop); b.player.SetTime(piTick(now),clocksB[*it].start,clocksB[*it].stop);
            a.player.Update(piTick(now),true); b.player.Update(piTick(now),true);
            auto sa=snapshot(a),sb=snapshot(b); pairErrors+=compare(sa,sb,"paused-seek",*it);
            historyErrors+=compare(baselineA[*it],sa,"reference-seek-history",*it,false); historyErrors+=compare(baselineB[*it],sb,"candidate-seek-history",*it,false); }
        ++now; a.player.Restart(piTick(now)); b.player.Restart(piTick(now));
        for(auto tick:ticks) { a.player.Update(piTick(now+tick)); b.player.Update(piTick(now+tick));
            auto sa=snapshot(a),sb=snapshot(b); pairErrors+=compare(sa,sb,"restart",tick);
            historyErrors+=compare(baselineA[tick],sa,"reference-restart-history",tick); historyErrors+=compare(baselineB[tick],sb,"candidate-restart-history",tick); }
        std::printf("[IMM_EVALUATED_COMPARE] layers=%zu mode-samples=%zu pair-errors=%u history-errors=%u excluded-reference=%u excluded-candidate=%u tolerance=1e-5 scope=paint-groups-root; geometry/audio/navigation/rendering not evaluated\n",a.layers.size(),ticks.size()*3,pairErrors,historyErrors,a.excluded,b.excluded);
        return (pairErrors || historyErrors)?1:0;
    } catch(const std::exception& e) { std::fprintf(stderr,"[IMM_EVALUATED_COMPARE] %s\n",e.what()); return 2; }
}
