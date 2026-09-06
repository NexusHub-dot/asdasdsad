#pragma once
#include "Core.hpp"
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>

namespace fwl {
inline std::string csvCell(std::string text) {
    std::string out="\"";
    for(char c:text){if(c=='\"')out+='\"';out+=c;}
    return out+'\"';
}
inline void writeWindowCSV(Run const& r,std::ostream& out) {
    out.imbue(std::locale::classic());out<<std::setprecision(17);
    out<<"input_index,source_event_index,tick,percentage,player,action,gamemode,active,ignored,measured,negative_boundary,positive_boundary,earliest_passing_offset,latest_passing_offset,width_positions,span_ticks,nominal_ms,early_censored,late_censored,histogram_above_20,negative_failure_tick,positive_failure_tick,negative_reason,positive_reason,checkpoint_used,checkpoint_tick,full_restart,fallback,uncertainty,physics_updates,success_endpoint_tick,settling_ticks,scope,final_validation_warning,warning\n";
    struct Summary {int cp=-1,tick=0;bool mixed=false,full=false;uint64_t updates=0;};
    std::vector<Summary> summaries(r.inputs.size());
    for(auto const& d:r.diagnostics)if(d.sourceIndex>=0 && static_cast<size_t>(d.sourceIndex)<summaries.size()) {
        auto& sum=summaries[d.sourceIndex];sum.updates+=d.physicsUpdates;sum.full|=d.fullRestart;
        if(d.checkpointIndex>=0){if(sum.cp>=0 && sum.cp!=d.checkpointIndex)sum.mixed=true;sum.cp=d.checkpointIndex;sum.tick=d.checkpointTick;}
    }
    size_t inputIndex=0;
    for(size_t i=0;i<r.inputs.size();++i) {
        auto const& a=r.inputs[i];auto w=i<r.windows.size()?r.windows[i]:Window{};
        bool target=!w.ignored && shouldMeasure(a,r.includeRelevantReleases);
        if(target)out<<++inputIndex;
        out<<','<<(a.sourceEventIndex>=0?a.sourceEventIndex:static_cast<int>(i))<<','<<a.tick<<',';
        if(a.percentage>=0)out<<a.percentage;
        out<<','<<a.player+1<<','<<(a.down?"press":"release")<<','<<inputModeName(a.mode)<<','<<a.active<<','<<!target<<','<<w.measured;
        out<<',';if(w.measured && !w.earlyCensored)out<<-w.early-1;
        out<<',';if(w.measured && !w.lateCensored)out<<w.late+1;
        if(w.measured)out<<','<<-w.early<<','<<w.late<<','<<w.width()<<','<<w.spanTicks()<<','<<w.nominalMilliseconds();
        else out<<",,,,,";
        out<<','<<w.earlyCensored<<','<<w.lateCensored<<','<<w.overflow<<',';
        if(w.earlyFailureTick>=0)out<<w.earlyFailureTick;
        out<<',';if(w.lateFailureTick>=0)out<<w.lateFailureTick;
        out<<','<<csvCell(w.earlyReason)<<','<<csvCell(w.lateReason);
        auto const& sum=summaries[i];int cp=sum.cp,cpTick=sum.tick;bool mixed=sum.mixed,full=sum.full;auto updates=sum.updates;
        out<<',';if(mixed)out<<"multiple";else if(cp>=0)out<<cp;
        out<<',';if(!mixed && cp>=0)out<<cpTick;
        out<<','<<full<<','<<w.checkpointFallback<<','<<(r.uncertain||w.unstable)<<','<<updates
            <<','<<trialEndpoint(r,i,r.nextClick)<<','<<r.settlingTicks<<','<<(r.nextClick?"next-input-plus-settling":"reference-end")
            <<','<<r.finalValidationWarning<<','<<csvCell(r.validationMessage)<<'\n';
    }
}
inline void writeTrialCSV(Run const& r,std::ostream& out) {
    out.imbue(std::locale::classic());
    out<<"source_event_index,offset,kind,passed,endpoint_tick,failure_tick,checkpoint_index,checkpoint_tick,full_restart,fallback,uncertain,physics_updates,completion,reason\n";
    for(auto const& d:r.diagnostics) {
        if(d.sourceIndex>=0 && static_cast<size_t>(d.sourceIndex)<r.inputs.size()) {
            auto src=r.inputs[d.sourceIndex].sourceEventIndex;out<<(src>=0?src:d.sourceIndex);
        }
        out<<','<<d.offset<<','<<d.kind<<','<<d.passed<<','<<d.endpointTick<<',';
        if(d.failureTick>=0)out<<d.failureTick;
        out<<',';if(d.checkpointIndex>=0)out<<d.checkpointIndex;
        out<<','<<d.checkpointTick<<','<<d.fullRestart<<','<<d.fallback<<','<<d.uncertain<<','<<d.physicsUpdates<<','<<d.completion<<','<<csvCell(d.reason)<<'\n';
    }
}
inline void writeHistogramCSV(Run const& r,std::ostream& out) {
    auto c=count(r,r.endTick,r.includeRelevantReleases);
    out<<"bucket,count\n";
    for(int i=1;i<=maxWindow;++i)out<<i<<','<<c.exact[i]<<'\n';
    for(auto pair:{std::pair{5,6},std::pair{7,8},std::pair{9,12},std::pair{13,20}}) {
        int sum=0;for(int i=pair.first;i<=pair.second;++i)sum+=c.exact[i];
        out<<pair.first<<'-'<<pair.second<<','<<sum<<'\n';
    }
    out<<">20,"<<c.above<<"\nunknown,"<<c.unknown<<"\npresses,"<<c.presses<<"\nreleases,"<<c.releases
        <<"\nall_targets,"<<c.presses+c.releases<<"\nraw_events,"<<r.inputs.size()<<'\n';
}
struct WindowDifference { int sourceIndex=-1;std::string field,a,b; };
inline std::vector<WindowDifference> compareWindows(Run const& a,Run const& b) {
    std::vector<WindowDifference> result;
    auto compare=[&](int index,std::string name,auto av,auto bv) {
        if(av!=bv){std::ostringstream x,y;x<<av;y<<bv;result.push_back({index,std::move(name),x.str(),y.str()});}
    };
    compare(-1,"fingerprint",a.fingerprint,b.fingerprint);compare(-1,"environment",a.environment,b.environment);
    compare(-1,"macro_revision",a.macroRevision,b.macroRevision);compare(-1,"seed",a.seed,b.seed);
    compare(-1,"replay_seed",a.replaySeed.value_or(a.seed),b.replaySeed.value_or(b.seed));
    compare(-1,"end_tick",a.endTick,b.endTick);compare(-1,"completed",a.completed,b.completed);
    compare(-1,"next_click",a.nextClick,b.nextClick);compare(-1,"settling_ticks",a.settlingTicks,b.settlingTicks);
    compare(-1,"releases",a.includeRelevantReleases,b.includeRelevantReleases);
    compare(-1,"final_validation_warning",a.finalValidationWarning,b.finalValidationWarning);
    compare(-1,"input_count",a.inputs.size(),b.inputs.size());compare(-1,"window_count",a.windows.size(),b.windows.size());
    for(size_t i=0;i<std::min(a.inputs.size(),b.inputs.size());++i) {
        compare(static_cast<int>(i),"tick",a.inputs[i].tick,b.inputs[i].tick);
        compare(static_cast<int>(i),"player",a.inputs[i].player,b.inputs[i].player);
        compare(static_cast<int>(i),"down",a.inputs[i].down,b.inputs[i].down);
        compare(static_cast<int>(i),"mode",int(a.inputs[i].mode),int(b.inputs[i].mode));
        if(i>=a.windows.size() || i>=b.windows.size())continue;
        auto const& x=a.windows[i];auto const& y=b.windows[i];
        compare(static_cast<int>(i),"early",x.early,y.early);compare(static_cast<int>(i),"late",x.late,y.late);
        compare(static_cast<int>(i),"width",x.width(),y.width());compare(static_cast<int>(i),"measured",x.measured,y.measured);
        compare(static_cast<int>(i),"ignored",x.ignored,y.ignored);compare(static_cast<int>(i),"overflow",x.overflow,y.overflow);
        compare(static_cast<int>(i),"early_censored",x.earlyCensored,y.earlyCensored);
        compare(static_cast<int>(i),"late_censored",x.lateCensored,y.lateCensored);
        compare(static_cast<int>(i),"early_failure_tick",x.earlyFailureTick,y.earlyFailureTick);
        compare(static_cast<int>(i),"late_failure_tick",x.lateFailureTick,y.lateFailureTick);
        compare(static_cast<int>(i),"unstable",x.unstable,y.unstable);
    }
    return result;
}
inline void writeComparisonCSV(Run const& a,Run const& b,std::ostream& out) {
    out<<"source_index,field,accelerated,full_restart\n";
    for(auto const& d:compareWindows(a,b))out<<d.sourceIndex<<','<<d.field<<','<<csvCell(d.a)<<','<<csvCell(d.b)<<'\n';
}
}
