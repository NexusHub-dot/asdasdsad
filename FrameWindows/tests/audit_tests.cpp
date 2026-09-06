#include "AnalysisCache.hpp"
#include "Diagnostics.hpp"
#include <cstdlib>
#include <iostream>
#include <random>
#include <chrono>
using namespace fwl;
int checks=0;
void require(bool value,char const* name){++checks;if(!value){std::cerr<<"FAIL: "<<name<<'\n';std::exit(1);}}

std::vector<size_t> order(Run const& r,size_t target,int offset,int start=0) {
    CandidatePlayback p;p.reset(timelineAt(r,start).inputCursor,target,offset);
    std::vector<size_t> result;while(auto i=p.next(r.inputs)){result.push_back(*i);p.consume(*i);}return result;
}
Window oracleWindow(Run const& r,bool local,int death) {
    Search s(1);
    while(!s.finished()) {
        bool inside=s.offset>=-3 && s.offset<=2;
        s.accept(s.baseline() || (inside && death>=trialEndpoint(r,0,local,s.offset)),inside?death:21);
    }
    return s.results[0];
}

// Synthetic discrete world, NOT a GD physics substitute. The whole world state
// is copied at checkpoints. It exercises the production overlay/search/selectors
// with held controls, deferred commands, RNG and moving obstacles.
struct World {
    std::array<int64_t,2> position{},velocity{};
    std::array<bool,2> held{},queuedRelease{};
    uint64_t rng=17;int moving=0;
    bool operator==(World const&)const=default;
    void step(int tick) {
        rng=rng*6364136223846793005ULL+1442695040888963407ULL;
        moving=int(rng>>60);
        for(int k=0;k<2;++k) {
            if(queuedRelease[k])held[k]=false;
            queuedRelease[k]=false;
            velocity[k]+=held[k]?2:-1;
            position[k]+=velocity[k]+moving;
        }
        if(tick%37==0)queuedRelease[1]=true;
    }
};
struct Point:AnalysisTimeline {World world;};
struct Fixture {
    Run run;std::vector<World> baseline;std::vector<Point> points;
    Fixture() {
        run.endTick=1500;run.nextClick=true;
        for(int t=100;t<1450;t+=45) {
            run.inputs.push_back({t,0,true,0,0,InputMode::Ship});
            run.inputs.push_back({t,1,true,0,0,InputMode::Wave});
            run.inputs.push_back({t+13,0,false,0,0,InputMode::Ship});
            run.inputs.push_back({t+16,1,false,0,0,InputMode::Wave});
        }
        auto plan=inputAwareCheckpointPlan(run);size_t cursor=0;World world;
        for(int tick=0;tick<run.endTick;++tick) {
            if(std::binary_search(plan.begin(),plan.end(),tick)) {
                Point cp;static_cast<AnalysisTimeline&>(cp)=timelineAt(run,tick);cp.world=world;cp.verified=true;points.push_back(cp);
            }
            while(cursor<run.inputs.size() && run.inputs[cursor].tick==tick) {
                auto const& in=run.inputs[cursor++];world.held[in.player]=in.down;
            }
            world.step(tick);baseline.push_back(world);
        }
    }
    Run analyze(bool accelerated,bool corrupt,uint64_t& updates) {
        auto output=run;Search search(run.inputs);auto cache=points;
        if(corrupt)for(size_t i=0;i<cache.size();i+=3){cache[i].world.rng^=4;cache[i].usable=false;}
        while(!search.finished()) {
            if(search.baseline()){search.accept(true);continue;}
            size_t target=search.index;int offset=search.offset;
            if(!canShift(run,target,offset)){search.accept(false);continue;}
            auto selected=accelerated?selectCandidateCheckpoint(cache,run,target,offset):std::nullopt;
            World world;int tick=0;
            if(selected){world=cache[*selected].world;tick=cache[*selected].tick;}
            CandidatePlayback playback;playback.reset(timelineAt(run,tick).inputCursor,target,offset);
            bool passed=true;int failure=-1;int end=trialEndpoint(run,target,true,offset);
            for(;tick<end;++tick) {
                while(auto index=playback.next(run.inputs)) {
                    if(playback.tick(run.inputs,*index)>tick)break;
                    require(playback.tick(run.inputs,*index)==tick,"synthetic scheduler never misses a tick");
                    auto const& in=run.inputs[*index];world.held[in.player]=in.down;playback.consume(*index);
                }
                world.step(tick);++updates;
                // Nearby obstacle compares to its moving canonical route; a
                // timing shift creates increasing, deterministic displacement.
                int64_t drift=std::abs(world.position[0]-baseline[tick].position[0])+std::abs(world.position[1]-baseline[tick].position[1]);
                if(drift>230){passed=false;failure=tick;break;}
            }
            search.accept(passed,failure);
        }
        output.windows=search.results;return output;
    }
};

int main() {
    Window six;six.early=3;six.late=2;six.measured=true;
    require(six.width()==6 && six.spanTicks()==5,"01 inclusive positions versus endpoint distance");
    require(std::abs(six.nominalMilliseconds()-25)<1e-10,"01 six nominal bins = 25ms; span = 20.833ms");
    Search search(2);search.accept(true);search.accept(true);search.accept(false,10);
    require(search.stage==Search::Stage::Late && search.offset==1,"02 first negative failure stops direction");
    search.accept(false,20);
    require(search.index==1 && search.offset==-1,"03 first positive failure advances target");
    auto unstable=resolveFailureConfirmation(true,19,-1);
    require(!unstable.stable && !unstable.acceptedPass && unstable.acceptedFailureTick==19,"04 disagreement cannot widen");
    Run r;r.endTick=500;r.inputs={{20,0,true,0,0,InputMode::Cube},{21,0,false,0,0,InputMode::Cube},
        {40,1,true,0,0,InputMode::Cube},{60,0,true,0,0,InputMode::Ship},{70,0,false,0,0,InputMode::Ship}};
    require(nextMeaningfulInput(r,0)==2 && trialEndpoint(r,0,true)==45,"05 next meaningful active group plus settling");
    require(order(r,0,5)==std::vector<size_t>({1,0,2,3,4}),"06 ignored release still replays at original tick");
    for(auto mode:{InputMode::Ship,InputMode::Wave,InputMode::Robot}) {
        Input in;in.mode=mode;require(shouldMeasure(in),"07 continuous-control release analyzed");
    }
    for(auto mode:{InputMode::Cube,InputMode::Ball,InputMode::Ufo,InputMode::Spider,InputMode::Swing}) {
        Input in;in.mode=mode;require(!shouldMeasure(in),"08 discrete-control release replay-only");
    }
    Run portal=r;portal.inputs[1].mode=InputMode::Robot;
    require(nextMeaningfulInput(portal,0)==1,"09 release classified by its own tick after portal");
    require(order(r,0,1)==std::vector<size_t>({0,1,2,3,4}),"10 press tied with release retains source order");
    require(order(r,1,39)==std::vector<size_t>({0,2,1,3,4}),"10 release tied with later press retains source order");
    require(order(r,3,-40)==std::vector<size_t>({0,3,1,2,4}),"11 earlier candidate crosses events, unrelated order unchanged");
    require(order(r,0,51)==std::vector<size_t>({1,2,3,4,0}),"11 later candidate crosses events, unrelated order unchanged");
    std::vector<AnalysisTimeline> cps(3);cps[0].tick=5;cps[1].tick=8;cps[2].tick=19;
    auto cp=selectCandidateCheckpoint(cps,r,0,-1);
    require(cp && cps[*cp].tick==5,"12 checkpoint precedes shifted AND original event with margin");
    cp=selectCandidateCheckpoint(cps,r,0,4);
    require(cp && cps[*cp].tick==8,"13 positive offset can use a closer safe checkpoint");
    require(candidateRestoreBoundary(r,0,0,0)==19,"13 zero requested safety still strictly before target");
    Fixture fixture;uint64_t fullUpdates=0,fastUpdates=0,fallbackUpdates=0;
    auto full=fixture.analyze(false,false,fullUpdates),fast=fixture.analyze(true,false,fastUpdates),fallback=fixture.analyze(true,true,fallbackUpdates);
    require(compareWindows(full,fast).empty() && compareWindows(full,fallback).empty(),"14 checkpoint and rejected-checkpoint fallback match every full-restart window");
    require(fastUpdates<fullUpdates && fallbackUpdates>=fastUpdates,"14 acceleration reduces synthetic physics work");
    require(fast.windows[0].measured && fast.windows[1].measured,"15 simultaneous dual P1/P2 independently analyzed");
    r.inputs[2].active=false;
    require(nextMeaningfulInput(r,0)==3,"16 dormant P2 excluded as endpoint/target");
    Pose expected;expected.dual=false;expected.values={10,20,3,100,200,30};
    expected.state=std::array<PlayerState,2>{};auto actual=expected;actual.values[3]=NAN;(*actual.state)[1].mode=InputMode::Ship;
    require(sameValidationPose(expected,actual),"16 dormant P2 values and flags ignored");
    expected.dual=true;actual.dual=true;require(!sameValidationPose(expected,actual),"15 active dual mismatch rejected");
    Search final(1);final.accept(true);final.accept(false);final.accept(false);final.fail("final mismatch");
    require(final.stage==Search::Stage::Done && final.results[0].measured && final.finalValidationWarning,"17 final failure preserves windows with warning");
    final.fail("another callback reported the same final failure");
    require(final.stage==Search::Stage::Done && final.finalValidationWarning,"17 repeated final failure callbacks cannot discard measurements");
    Search opening(1);opening.fail("opening mismatch");require(opening.stage==Search::Stage::Failed && !opening.results[0].measured,"18 opening failure fatal");
    Search wide(1);std::vector<int> visited;
    while(!wide.finished()){if(!wide.baseline())visited.push_back(wide.offset);wide.accept(true);}
    require(wide.results[0].width()==41 && wide.results[0].earlyCensored && wide.results[0].lateCensored && visited.size()==40,"19 capped search still measures both sides");
    Run dense;dense.endTick=100;for(int i=0;i<99;++i)dense.inputs.push_back({i,i%2,bool(i%2),0,0,InputMode::Wave});
    require(order(dense,20,20).size()==99 && trialEndpoint(dense,20,true,20)==45,"20 dense sequence late target always executes before endpoint");
    require(trialEndpoint(dense,98,true)==100,"21 final input uses reference end");
    dense.completed=true;
    require(!trialSucceeded(dense,100,100,false,false),"22 completed reference requires terminal signal");
    require(trialSucceeded(dense,100,100,false,true),"22 natural completion passes");
    require(!trialSucceeded(dense,100,100,true,true),"22 death wins over completion");
    require(!trialSucceeded(dense,100,100,false,true,false),"22 completion cannot pass an undelivered shifted event");
    dense.completed=false;require(trialSucceeded(dense,100,100,false,false),"22 prefix only requires survival");
    Run endpoint;endpoint.endTick=500;endpoint.inputs={{20,0,true,0,0,InputMode::Cube},{100,0,true,0,0,InputMode::Cube}};
    auto settled=oracleWindow(endpoint,true,102);endpoint.settlingTicks=0;auto executeOnly=oracleWindow(endpoint,true,102);
    require(settled.width()==1 && executeOnly.width()==6,"23 executing next action alone misses a failure two steps later");
    endpoint.settlingTicks=4;
    require(oracleWindow(endpoint,true,400).width()==6 && oracleWindow(endpoint,false,400).width()==1,"24 unrelated late death narrows full-route metric only");
    // Tolerance edge cases and immutable canonical contract.
    expected.dual=false;actual=expected;actual.values[0]+=0.0001;
    require(sameValidationPose(expected,actual),"small position noise accepted");
    actual.values[2]+=0.0001;require(!sameValidationPose(expected,actual),"velocity mismatch uses its own tolerance");
    actual=expected;(*actual.state)[0].mode=InputMode::Ship;require(!sameValidationPose(expected,actual),"mode mismatch with identical position rejected");
    actual=expected;(*actual.state)[0].flags=1;require(!sameValidationPose(expected,actual),"gravity/physics flags exact");
    actual=expected;expected.seeds=std::array<uint64_t,2>{1,2};actual.seeds=std::array<uint64_t,2>{1,3};
    require(!sameValidationPose(expected,actual),"RNG mismatch rejected");
    require(expected.values[0]==10,"validation never rewrites expected pose");
    Run exportRun;exportRun.endTick=500;exportRun.inputs={{20,0,true,0,0,InputMode::Cube}};exportRun.windows={six};
    exportRun.finalValidationWarning=true;exportRun.uncertain=true;exportRun.validationMessage="warning, \"quoted\"";
    std::ostringstream csv,hist;writeWindowCSV(exportRun,csv);writeHistogramCSV(exportRun,hist);
    require(csv.str().find("\"warning, \"\"quoted\"\"\"")!=std::string::npos,"CSV warnings escaped");
    require(hist.str().find("6,1\n")!=std::string::npos,"inclusive width used in histogram export");
    require(!precision(exportRun,500),"unverified run cannot claim precision");
    auto altered=fast;altered.windows[0].early++;
    require(!compareWindows(fast,altered).empty(),"A/B diff catches equal-looking histogram boundary errors");
    std::cout<<"24 requested scenarios plus numerical/export checks passed ("<<checks<<" assertions).\n"
        <<"Synthetic physics updates: full="<<fullUpdates<<", accelerated="<<fastUpdates<<", fallback="<<fallbackUpdates<<". This is NOT a GD benchmark.\n";
}
