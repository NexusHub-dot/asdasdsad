#include "Storage.hpp"
#include "Diagnostics.hpp"
#include <Geode/Geode.hpp>
#include <fstream>
#include <sstream>
using namespace geode::prelude;
namespace fwl {
std::filesystem::path dataPath(std::string const& key) {
    return Mod::get()->getSaveDir() / "frame-windows" / (key + ".fwl.json");
}
bool save(Run const& r, std::string& error, std::string const& tag) {
    try {
        auto j = matjson::Value::object();
        j["schema"] = 2; j["tickRate"] = 240; j["fingerprint"] = r.fingerprint;
        j["measurementMode"]=r.nextClick?"next-input-plus-settling":"reference-end-survival";j["clock"]="physics-steps";
        j["analysisVersion"]=analysisVersion;
        j["widthConvention"]="discrete-passing-positions-inclusive";
        j["settlingTicks"]=r.settlingTicks;
        j["finalValidationWarning"]=r.finalValidationWarning;j["uncertain"]=r.uncertain;
        j["validationMessage"]=r.validationMessage;j["forceFullRestart"]=r.forceFullRestart;
        auto metrics=matjson::Value::object();
        metrics["physicsUpdates"]=static_cast<int64_t>(r.metrics.physicsUpdates);
        metrics["updateCalls"]=static_cast<int64_t>(r.metrics.updateCalls);
        metrics["checkpointRestores"]=static_cast<int64_t>(r.metrics.checkpointRestores);
        metrics["fullRestarts"]=static_cast<int64_t>(r.metrics.fullRestarts);
        metrics["fallbacks"]=static_cast<int64_t>(r.metrics.fallbacks);
        metrics["candidates"]=static_cast<int64_t>(r.metrics.candidates);
        metrics["successfulCandidates"]=static_cast<int64_t>(r.metrics.successful);
        metrics["failedCandidates"]=static_cast<int64_t>(r.metrics.failed);
        metrics["doubleChecks"]=static_cast<int64_t>(r.metrics.doubleChecks);
        metrics["elapsedSeconds"]=r.metrics.elapsedSeconds;j["metrics"]=metrics;
        j["trials"]=matjson::Value::array();
        for(auto const& d:r.diagnostics) {
            auto v=matjson::Value::object();
            v["sourceIndex"]=d.sourceIndex;v["offset"]=d.offset;v["endpointTick"]=d.endpointTick;v["failureTick"]=d.failureTick;
            v["checkpointIndex"]=d.checkpointIndex;v["checkpointTick"]=d.checkpointTick;
            v["physicsUpdates"]=static_cast<int64_t>(d.physicsUpdates);
            v["passed"]=d.passed;v["completion"]=d.completion;v["fullRestart"]=d.fullRestart;
            v["fallback"]=d.fallback;v["uncertain"]=d.uncertain;v["kind"]=d.kind;v["reason"]=d.reason;
            j["trials"].push(v);
        }
        auto totals=count(r,r.endTick,r.includeRelevantReleases);
        j["histogram"]=matjson::Value::array();for(auto n:totals.exact)j["histogram"].push(n);
        j["above20"]=totals.above;j["unknown"]=totals.unknown;
        j["modVersion"]=Mod::get()->getVersion().toVString();j["gdVersion"]="2.2081";
        j["levelName"] = r.levelName; j["endTick"] = r.endTick; j["completed"] = r.completed;
        j["pulseInputs"] = r.pulseInputs;
        j["includeRelevantReleases"] = r.includeRelevantReleases;
        j["macroRevision"] = r.macroRevision;
        j["seed"] = std::to_string(r.seed); j["environment"] = r.environment;
        if(r.replaySeed)j["replaySeed"]=std::to_string(*r.replaySeed);
        j["inputs"] = matjson::Value::array();
        for (size_t k=0;k<r.inputs.size();++k) {
            auto const& in=r.inputs[k]; auto v=matjson::Value::object();
            v["tick"]=in.tick; v["player"]=in.player; v["down"]=in.down; v["x"]=in.x; v["y"]=in.y;
            v["mode"]=inputModeName(in.mode);v["active"]=in.active;
            v["sourceEventIndex"]=in.sourceEventIndex;v["percentage"]=in.percentage;
            if(k<r.windows.size()) {
                auto const& w=r.windows[k]; v["early"]=w.early; v["late"]=w.late;
                v["measured"]=w.measured; v["overflow"]=w.overflow;
                v["earlyCensored"]=w.earlyCensored;v["lateCensored"]=w.lateCensored;
                v["endpointTick"]=w.endpointTick;v["earlyReason"]=w.earlyReason;v["lateReason"]=w.lateReason;
                if(w.measured){v["widthPositions"]=w.width();v["spanTicks"]=w.spanTicks();
                    v["earliestPassingOffset"]=-w.early;v["latestPassingOffset"]=w.late;}
                v["earlyFailureTick"]=w.earlyFailureTick;v["lateFailureTick"]=w.lateFailureTick;
                v["unstable"]=w.unstable;v["checkpointFallback"]=w.checkpointFallback;v["ignored"]=w.ignored;
                if(w.measured){v["testedEarliest"]=in.tick-w.early;v["testedLatest"]=in.tick+w.late;v["nominalMs"]=w.nominalMilliseconds();}
            }
            j["inputs"].push(v);
        }
        j["poses"]=matjson::Value::array();
        for(auto const& p:r.poses) {
            auto v=matjson::Value::object(); v["tick"]=p.tick; v["values"]=matjson::Value::array();
            for(double x:p.values) v["values"].push(x);
            if(p.dual)v["dual"]=*p.dual;
            if(p.state) {
                v["state"]=matjson::Value::array();
                for(auto const& a:*p.state) {
                    auto q=matjson::Value::object();q["mode"]=inputModeName(a.mode);q["flags"]=std::to_string(a.flags);
                    q["size"]=a.size;q["speed"]=a.speed;q["gravity"]=a.gravity;q["xVelocity"]=a.xVelocity;v["state"].push(q);
                }
            }
            if(p.seeds){v["seed"]=std::to_string((*p.seeds)[0]);v["replaySeed"]=std::to_string((*p.seeds)[1]);}
            j["poses"].push(v);
        }
        // Compact JSON keeps detailed trial evidence practical for dense runs.
        // Refuse oversize output before replacing a readable existing reference.
        auto serialized=j.dump();
        if(serialized.size()>256ull*1024*1024)throw std::runtime_error("Analysis JSON exceeds 256 MB; export CSV diagnostics and use a shorter reference");
        auto path=dataPath(r.fingerprint+tag), temp=path; temp += ".tmp";
        std::filesystem::create_directories(path.parent_path());
        { std::ofstream out(temp,std::ios::binary); out << serialized;out.flush(); if(!out) throw std::runtime_error("Unable to write run file"); }
        // Windows replace without deleting the existing reference first.
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Unable to replace run file");
        return true;
    } catch(std::exception const& e) { error=e.what(); return false; }
}
std::optional<Run> load(std::string const& fingerprint,std::string& error) {
    try {
        auto path=dataPath(fingerprint); if(!std::filesystem::exists(path)) return {};
        if(std::filesystem::file_size(path)>256ull*1024*1024) throw std::runtime_error("Run file exceeds 256 MB");
        std::ifstream in(path); std::string text((std::istreambuf_iterator<char>(in)),{});
        auto parsed=matjson::parse(text); if(!parsed) throw std::runtime_error("Invalid run JSON");
        auto& j=parsed.unwrap();
        auto integer=[&](matjson::Value const& v,char const* key) { auto n=v[key].asInt(); if(!n) throw std::runtime_error("Invalid integer in run"); return n.unwrap(); };
        if(integer(j,"schema")!=2 || integer(j,"tickRate")!=240) throw std::runtime_error("This reference uses the old progress-counter clock. Record a new reference with F6.");
        Run r; r.fingerprint=j["fingerprint"].asString().unwrapOr("");
        if(r.fingerprint!=fingerprint) throw std::runtime_error("Wrong level fingerprint");
        r.levelName=j["levelName"].asString().unwrapOr(""); r.environment=j["environment"].asString().unwrapOr("");
        r.endTick=static_cast<int>(integer(j,"endTick")); r.completed=j["completed"].asBool().unwrapOr(false);
        r.pulseInputs=j["pulseInputs"].asBool().unwrapOr(false);
        r.includeRelevantReleases=j["includeRelevantReleases"].asBool().unwrapOr(true);
        r.macroRevision=j["macroRevision"].asString().unwrapOr("");
        auto scope=j["measurementMode"].asString().unwrapOr("");
        r.nextClick=scope=="next-click-survival" || scope=="next-control-survival" || scope=="next-input-plus-settling";
        bool currentAnalysis=j["analysisVersion"].asInt().unwrapOr(0)==analysisVersion;
        r.settlingTicks=static_cast<int>(j["settlingTicks"].asInt().unwrapOr(defaultSettlingTicks));
        if(r.settlingTicks<0 || r.settlingTicks>240)throw std::runtime_error("Invalid settling horizon");
        r.finalValidationWarning=j["finalValidationWarning"].asBool().unwrapOr(false);
        r.uncertain=j["uncertain"].asBool().unwrapOr(false);r.validationMessage=j["validationMessage"].asString().unwrapOr("");
        r.forceFullRestart=j["forceFullRestart"].asBool().unwrapOr(false);
        auto const& metrics=j["metrics"];
        auto metric=[&](char const* key){return static_cast<uint64_t>(std::max<int64_t>(0,metrics[key].asInt().unwrapOr(0)));};
        r.metrics.physicsUpdates=metric("physicsUpdates");r.metrics.updateCalls=metric("updateCalls");
        r.metrics.checkpointRestores=metric("checkpointRestores");r.metrics.fullRestarts=metric("fullRestarts");
        r.metrics.fallbacks=metric("fallbacks");r.metrics.candidates=metric("candidates");
        r.metrics.successful=metric("successfulCandidates");r.metrics.failed=metric("failedCandidates");
        r.metrics.doubleChecks=metric("doubleChecks");r.metrics.elapsedSeconds=metrics["elapsedSeconds"].asDouble().unwrapOr(0);
        if(auto trials=j["trials"].asArray();currentAnalysis && trials)for(auto const& v:trials.unwrap()) {
            TrialDiagnostic d;
            d.sourceIndex=static_cast<int>(v["sourceIndex"].asInt().unwrapOr(-1));d.offset=static_cast<int>(v["offset"].asInt().unwrapOr(0));
            d.endpointTick=static_cast<int>(v["endpointTick"].asInt().unwrapOr(0));d.failureTick=static_cast<int>(v["failureTick"].asInt().unwrapOr(-1));
            d.checkpointIndex=static_cast<int>(v["checkpointIndex"].asInt().unwrapOr(-1));d.checkpointTick=static_cast<int>(v["checkpointTick"].asInt().unwrapOr(0));
            d.physicsUpdates=static_cast<uint64_t>(std::max<int64_t>(0,v["physicsUpdates"].asInt().unwrapOr(0)));
            d.passed=v["passed"].asBool().unwrapOr(false);d.completion=v["completion"].asBool().unwrapOr(false);
            d.fullRestart=v["fullRestart"].asBool().unwrapOr(true);d.fallback=v["fallback"].asBool().unwrapOr(false);
            d.uncertain=v["uncertain"].asBool().unwrapOr(false);d.kind=v["kind"].asString().unwrapOr("");d.reason=v["reason"].asString().unwrapOr("");
            r.diagnostics.push_back(std::move(d));
        }
        if(r.endTick<=0 || r.endTick>240*60*60*4) throw std::runtime_error("Invalid reference duration");
        r.seed=std::stoull(j["seed"].asString().unwrapOr("0"));
        if(auto replay=j["replaySeed"].asString())r.replaySeed=std::stoull(replay.unwrap());
        auto inputs=j["inputs"].asArray(); if(!inputs || inputs.unwrap().size()>100000) throw std::runtime_error("Invalid inputs");
        int previous=-1;
        for(auto const& v:inputs.unwrap()) {
            Input a; a.tick=static_cast<int>(integer(v,"tick")); a.player=static_cast<int>(integer(v,"player"));
            a.down=v["down"].asBool().unwrapOr(false); a.x=static_cast<float>(v["x"].asDouble().unwrapOr(0)); a.y=static_cast<float>(v["y"].asDouble().unwrapOr(0));
            auto modeName=v["mode"].asString().unwrapOr("");
            if(modeName.empty() && v["wave"].asBool().unwrapOr(false))modeName="wave";
            a.mode=inputModeFromName(modeName);a.active=v["active"].asBool().unwrapOr(true);
            a.sourceEventIndex=static_cast<int>(v["sourceEventIndex"].asInt().unwrapOr(static_cast<int64_t>(r.inputs.size())));
            a.percentage=v["percentage"].asDouble().unwrapOr(-1);
            if(a.tick<previous || a.tick<0 || a.tick>=r.endTick || a.player<0 || a.player>1 || !std::isfinite(a.x) || !std::isfinite(a.y)) throw std::runtime_error("Invalid input timeline");
            previous=a.tick; r.inputs.push_back(a);
            Window w; w.early=static_cast<int>(v["early"].asInt().unwrapOr(0)); w.late=static_cast<int>(v["late"].asInt().unwrapOr(0));
            w.measured=v["measured"].asBool().unwrapOr(false); w.overflow=v["overflow"].asBool().unwrapOr(false);
            w.earlyFailureTick=static_cast<int>(v["earlyFailureTick"].asInt().unwrapOr(-1));
            w.lateFailureTick=static_cast<int>(v["lateFailureTick"].asInt().unwrapOr(-1));
            w.unstable=v["unstable"].asBool().unwrapOr(false);
            w.checkpointFallback=v["checkpointFallback"].asBool().unwrapOr(false);
            w.ignored=v["ignored"].asBool().unwrapOr(false);
            w.earlyCensored=v["earlyCensored"].asBool().unwrapOr(false);w.lateCensored=v["lateCensored"].asBool().unwrapOr(false);
            w.endpointTick=static_cast<int>(v["endpointTick"].asInt().unwrapOr(0));
            w.earlyReason=v["earlyReason"].asString().unwrapOr("");w.lateReason=v["lateReason"].asString().unwrapOr("");
            if(w.early<0 || w.late<0 || w.early>20 || w.late>20 || (w.measured && w.overflow!=(w.width()>20))) throw std::runtime_error("Invalid measured window");
            r.windows.push_back(currentAnalysis?w:Window{});
        }
        auto poses=j["poses"].asArray(); if(!poses || poses.unwrap().size()>100000) throw std::runtime_error("Missing validation poses");
        for(auto const& v:poses.unwrap()) {
            Pose p; p.tick=static_cast<int>(integer(v,"tick"));
            auto values=v["values"].asArray(); if(!values || values.unwrap().size()!=6) throw std::runtime_error("Invalid validation pose");
            for(int k=0;k<6;++k) { p.values[k]=values.unwrap()[k].asDouble().unwrapOr(NAN); if(!std::isfinite(p.values[k])) throw std::runtime_error("Invalid pose value"); }
            if(auto dual=v["dual"].asBool())p.dual=dual.unwrap();
            if(auto states=v["state"].asArray()) {
                if(states.unwrap().size()!=2)throw std::runtime_error("Invalid player state");
                p.state=std::array<PlayerState,2>{};
                for(int k=0;k<2;++k) {
                    auto const& q=states.unwrap()[k];auto& a=(*p.state)[k];
                    a.mode=inputModeFromName(q["mode"].asString().unwrapOr(""));a.flags=std::stoull(q["flags"].asString().unwrapOr("0"));
                    a.size=q["size"].asDouble().unwrapOr(NAN);a.speed=q["speed"].asDouble().unwrapOr(NAN);
                    a.gravity=q["gravity"].asDouble().unwrapOr(NAN);a.xVelocity=q["xVelocity"].asDouble().unwrapOr(NAN);
                    if(!std::isfinite(a.size)||!std::isfinite(a.speed)||!std::isfinite(a.gravity)||!std::isfinite(a.xVelocity))throw std::runtime_error("Invalid physics scalar");
                }
            }
            if(auto seed=v["seed"].asString())p.seeds=std::array<uint64_t,2>{std::stoull(seed.unwrap()),std::stoull(v["replaySeed"].asString().unwrapOr("0"))};
            if(p.tick<0 || p.tick>r.endTick || (!r.poses.empty() && p.tick<=r.poses.back().tick)) throw std::runtime_error("Invalid pose order");
            r.poses.push_back(p);
        }
        if(!currentAnalysis)error="Reference loaded. Re-analyze to replace the old widths with the new timing checks.";
        return r;
    }catch(std::exception const& e) { error=e.what(); return {}; }
}
bool exportCSV(Run const& r,std::string& error,std::string const& tag) {
    try {
        auto dir=Mod::get()->getSaveDir()/"frame-windows";
        std::filesystem::create_directories(dir);
        auto write=[&](char const* suffix,auto fn) {
            std::ofstream out(dir/(r.fingerprint+tag+suffix));fn(r,out);
            if(!out)throw std::runtime_error("Unable to export diagnostics CSV");
        };
        write(".csv",writeWindowCSV);write(".trials.csv",writeTrialCSV);write(".histogram.csv",writeHistogramCSV);
        return true;
    }catch(std::exception const& e) {error=e.what();return false;}
}
}
