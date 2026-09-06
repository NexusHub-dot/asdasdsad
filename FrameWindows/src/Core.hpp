#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fwl {
constexpr int maxWindow = 20;
constexpr double tickRate = 240.0;
constexpr int analysisVersion = 6;
constexpr int defaultSettlingTicks = 4; // 16.67 ms AFTER the anchor input step; policy, not a physics constant.
// Schema 2 identifies this clock/measurement format. UI-only Pulse updates
// should not invalidate otherwise matching saved references.
inline std::string compatibleEnvironment(std::string environment) {
    auto begin=environment.find(";alan.pulse_macro@");
    if(begin!=std::string::npos) {
        auto end=environment.find(';',begin+1);
        environment.erase(begin,end==std::string::npos?std::string::npos:end-begin);
    }
    return environment;
}
inline std::string pulseSlotKey(std::string const& fingerprint,int slot) {
    if(slot<1 || slot>20)throw std::runtime_error("Invalid Pulse slot");
    return fingerprint+"-pulse-"+std::to_string(slot);
}
enum class InputMode : std::uint8_t { Unknown=0, Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing };
inline char const* inputModeName(InputMode mode) {
    switch(mode) {
        case InputMode::Cube:return "cube"; case InputMode::Ship:return "ship"; case InputMode::Ball:return "ball";
        case InputMode::Ufo:return "ufo"; case InputMode::Wave:return "wave"; case InputMode::Robot:return "robot";
        case InputMode::Spider:return "spider"; case InputMode::Swing:return "swing"; default:return "unknown";
    }
}
inline InputMode inputModeFromName(std::string const& name) {
    if(name=="cube")return InputMode::Cube;if(name=="ship")return InputMode::Ship;if(name=="ball")return InputMode::Ball;
    if(name=="ufo")return InputMode::Ufo;if(name=="wave")return InputMode::Wave;if(name=="robot")return InputMode::Robot;
    if(name=="spider")return InputMode::Spider;if(name=="swing")return InputMode::Swing;return InputMode::Unknown;
}
inline bool releaseHasIndependentTiming(InputMode mode) {
    // Ship, wave and robot are continuous hold controls: release timing changes
    // the trajectory directly. Cube, ball, UFO, spider and swing releases only
    // end a latch and are replayed but are not separate frame-window timings.
    return mode==InputMode::Ship || mode==InputMode::Wave || mode==InputMode::Robot;
}
struct Input {
    int tick = 0;
    int player = 0;
    bool down = false;
    float x = 0, y = 0;
    InputMode mode=InputMode::Unknown;
    bool active=true; // P2 outside dual is replay-only.
    int sourceEventIndex=-1;
    double percentage=-1; // Native level percentage at the original input.
};
inline bool shouldMeasure(Input const& in,bool includeRelevantReleases=true) {
    if(!in.active)return false;
    if(in.down)return true;
    if(!includeRelevantReleases)return false;
    // Unknown is legacy metadata. Keep it measurable until the opening baseline
    // learns the actual mode, then applyEligibility() removes discrete releases.
    return in.mode==InputMode::Unknown || releaseHasIndependentTiming(in.mode);
}
struct PlayerState {
    InputMode mode=InputMode::Unknown;
    uint64_t flags=0;
    double size=1, speed=0, gravity=1, xVelocity=0;
};
struct Pose {
    int tick = 0;
    std::array<double, 6> values{}; // x, y, y-velocity for each player
    // Added in Pulse 1.3.6. Older saved references do not have this field;
    // in that case validation falls back to the replay's current dual state.
    std::optional<bool> dual;
    std::optional<std::array<PlayerState,2>> state;
    std::optional<std::array<uint64_t,2>> seeds;
};
// Read-only determinism guards. Preserve the existing 0.1-unit legacy position
// allowance; use 1e-6 for velocity and physics scalars. Neither tolerance changes
// gameplay. Live tolerance calibration remains a separate validation task.
inline bool sameValidationPose(Pose const& expected, Pose const& actual, double tolerance = 0.1) {
    // A dual-state disagreement is a real replay mismatch when the reference
    // recorded that metadata. Legacy references intentionally omit it.
    if (expected.dual && actual.dual && *expected.dual != *actual.dual) return false;

    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(expected.values[k]) || !std::isfinite(actual.values[k]) ||
            std::abs(expected.values[k] - actual.values[k]) > (k%3==2?1e-6:tolerance)) return false;
    }

    // Player 2 is dormant outside dual mode and GD is free to leave its
    // coordinates/velocity in a different placeholder state after a reset.
    // Comparing those dormant values caused false "replay differs" aborts.
    // In dual mode P2 remains part of deterministic validation.
    bool checkPlayer2 = actual.dual.value_or(expected.dual.value_or(true));
    if (checkPlayer2) {
        for (int k = 3; k < 6; ++k) {
            if (!std::isfinite(expected.values[k]) || !std::isfinite(actual.values[k]) ||
                std::abs(expected.values[k] - actual.values[k]) > (k%3==2?1e-6:tolerance)) return false;
        }
    }
    if(expected.state) {
        if(!actual.state)return false;
        for(int k=0;k<(checkPlayer2?2:1);++k) {
            auto const& a=(*expected.state)[k];auto const& b=(*actual.state)[k];
            if(a.mode!=b.mode || a.flags!=b.flags)return false;
            for(auto pair: {std::pair{a.size,b.size},std::pair{a.speed,b.speed},
                    std::pair{a.gravity,b.gravity},std::pair{a.xVelocity,b.xVelocity}})
                if(!std::isfinite(pair.first) || !std::isfinite(pair.second) || std::abs(pair.first-pair.second)>1e-6)return false;
        }
    }
    if(expected.seeds && (!actual.seeds || *expected.seeds!=*actual.seeds))return false;
    return true;
}
struct Window {
    int early = 0, late = 0;
    bool measured = false;
    bool overflow = false;
    bool earlyCensored=false, lateCensored=false;
    int endpointTick=0;
    std::string earlyReason,lateReason;
    int earlyFailureTick=-1,lateFailureTick=-1;
    bool unstable=false, checkpointFallback=false, ignored=false;
    std::string warning() const {
        if(unstable && checkpointFallback)return "WARNING: unstable double-check; checkpoint fallback used";
        if(unstable)return "WARNING: unstable double-check";
        if(checkpointFallback)return "Checkpoint fallback used";
        return {};
    }
    // Number of passing DISCRETE positions, not endpoint distance.
    int width() const { return early + late + 1; }
    int spanTicks() const { return early + late; }
    double nominalMilliseconds() const { return width()*1000.0/tickRate; }
};
struct TrialDiagnostic {
    int sourceIndex=-1, offset=0, endpointTick=0, failureTick=-1;
    int checkpointIndex=-1, checkpointTick=0;
    uint64_t physicsUpdates=0;
    bool passed=false, completion=false, fullRestart=true, fallback=false, uncertain=false;
    std::string kind,reason;
};
struct AnalysisMetrics {
    uint64_t physicsUpdates=0, updateCalls=0, checkpointRestores=0, fullRestarts=0, fallbacks=0;
    uint64_t candidates=0, successful=0, failed=0, doubleChecks=0;
    double elapsedSeconds=0;
};
struct Run {
    std::string fingerprint, levelName;
    std::vector<Input> inputs;
    std::vector<Pose> poses;
    std::vector<Window> windows;
    int endTick = 0;
    bool completed = false;
    bool pulseInputs = false;
    std::string macroRevision;
    uint64_t seed = 0;
    std::optional<uint64_t> replaySeed;
    std::string environment;
    bool nextClick=false;
    bool includeRelevantReleases=true;
    int settlingTicks=defaultSettlingTicks;
    bool finalValidationWarning=false, uncertain=false, forceFullRestart=false;
    std::string validationMessage;
    AnalysisMetrics metrics;
    std::vector<TrialDiagnostic> diagnostics;
};
// Treat simultaneous P1/P2 inputs as one time group. Both players share world
// state: the next meaningful group on EITHER active player is the local anchor.
inline std::optional<size_t> nextMeaningfulInput(Run const& run,size_t index) {
    if(index>=run.inputs.size())return {};
    for(size_t j=index+1;j<run.inputs.size();++j)
        if(run.inputs[j].tick>run.inputs[index].tick && shouldMeasure(run.inputs[j],run.includeRelevantReleases))return j;
    return {};
}
// Endpoint is exclusive: step T executes its inputs, then physics, then the
// clock becomes T+1. Run the anchor step AND settlingTicks subsequent steps.
// If a late candidate crosses the anchor, extend to settle after that candidate
// too; never accept a trial that stopped before its shifted event was delivered.
inline int trialEndpoint(Run const& run,size_t index,bool nextClick,int offset=0) {
    if(!nextClick || index>=run.inputs.size())return run.endTick;
    auto next=nextMeaningfulInput(run,index);
    if(!next)return run.endTick;
    int64_t anchor=std::max<int64_t>(run.inputs[*next].tick,static_cast<int64_t>(run.inputs[index].tick)+offset);
    return static_cast<int>(std::min<int64_t>(run.endTick,anchor+1+std::clamp(run.settlingTicks,0,240)));
}
inline bool needsCompletion(Run const& run,int endpoint) {return run.completed && endpoint==run.endTick;}
inline bool trialSucceeded(Run const& run,int endpoint,int reached,bool died,bool completed,bool targetDelivered=true) {
    return !died && targetDelivered && (completed || (reached>=endpoint && !needsCompletion(run,endpoint)));
}
inline bool repeatableFailure(bool passed,int firstTick,int repeatedTick) {
    return !passed && firstTick==repeatedTick;
}
struct FailureConfirmation {
    bool stable = true;
    bool acceptedPass = false;
    int acceptedFailureTick = -1;
};
// A failed boundary is replayed once using the same batched fixed-240-Hz
// engine path as ordinary trials. If the double-check disagrees, continue
// conservatively instead of aborting the full
// analysis: treat the boundary as failed using the first observed failure tick
// and flag the result as potentially inaccurate. This can only make a measured
// window narrower, never falsely widen it.
inline FailureConfirmation resolveFailureConfirmation(bool repeatedPassed,int firstTick,int repeatedTick) {
    if (repeatableFailure(repeatedPassed,firstTick,repeatedTick))
        return {true,false,repeatedTick};
    return {false,false,firstTick >= 0 ? firstTick : repeatedTick};
}
struct ReferenceCheckpoint {
    size_t inputs=0,poses=0;
    int physicsTick=0,recordedThrough=0,lastPoseTick=-1;
    std::array<bool,2> held{};
    bool invalid=false;
    std::string reason;
    std::uint64_t ordinal=0;
};
// Re-entering a level must resolve the same slot-qualified key used by Begin.
// Never fall back to a different take when a macro is selected.
template<class Loader>
std::optional<Run> loadSelectedReference(std::string const& level,int slot,
    std::string const& revision,Loader&& loader,std::string& error) {
    error.clear();
    auto run=loader(pulseSlotKey(level,slot),error);
    if(run && (revision.empty() || run->macroRevision!=revision)) {
        error="The selected macro changed since its reference. Record both with F6.";
        return {};
    }
    if(!run && revision.empty() && error.empty())return loader(level,error);
    if(!run && error.empty())error="No frame reference saved for this slot. Use F6, then F8 to save both.";
    return run;
}
inline void rewindReference(Run& run,ReferenceCheckpoint const& point) {
    if(point.inputs>run.inputs.size() || point.poses>run.poses.size() || point.physicsTick<0)
        throw std::runtime_error("Frame reference checkpoint is outside this recording");
    run.inputs.resize(point.inputs);run.poses.resize(point.poses);run.windows.clear();
    run.completed=false;run.endTick=point.recordedThrough;run.macroRevision.clear();
}

// Move exactly one transition. Crossing another transition is a physics test,
// not an automatic failure. The immutable source index breaks same-tick ties.
inline bool canShift(Run const& run, size_t index, int offset) {
    if(index>=run.inputs.size())return false;
    int64_t target=static_cast<int64_t>(run.inputs[index].tick)+offset;
    return target>=0 && target<run.endTick;
}
inline std::vector<Input> shifted(Run const& run, size_t index, int offset) {
    auto result = run.inputs;
    if (index < result.size()) result[index].tick += offset;
    std::stable_sort(result.begin(), result.end(), [](auto const& a, auto const& b) { return a.tick < b.tick; });
    return result;
}

// A trial is a complete replay from the same seed, with all other inputs fixed.
// Each side is searched until its first failure. A censored width is never put
// into an exact bin. Baselines are checked before AND after the candidate suite.
class Search {
public:
    enum class Stage { Baseline, Early, Late, FinalBaseline, Done, Failed };
    Stage stage = Stage::Baseline;
    size_t index = 0;
    int offset = 0, trials = 0;
    std::vector<Window> results;
    std::string error;
    bool finalValidationWarning = false;
    std::string finalValidationMessage;
    explicit Search(size_t count) : results(count) {}
    explicit Search(std::vector<Input> const& inputs,bool includeRelevantReleases=true) : results(inputs.size()) { applyEligibility(inputs,includeRelevantReleases); }
    bool baseline() const { return stage == Stage::Baseline || stage == Stage::FinalBaseline; }
    bool finished() const { return stage == Stage::Done || stage == Stage::Failed; }
    bool allMeasured() const {
        return std::all_of(results.begin(), results.end(), [](Window const& w) { return w.measured || w.ignored; });
    }
    size_t measuredCount() const {
        return static_cast<size_t>(std::count_if(results.begin(),results.end(),[](Window const& w){return w.measured;}));
    }
    size_t targetCount() const {
        return static_cast<size_t>(std::count_if(results.begin(),results.end(),[](Window const& w){return !w.ignored;}));
    }
    void applyEligibility(std::vector<Input> const& inputs,bool includeRelevantReleases=true) {
        if(inputs.size()!=results.size() || stage!=Stage::Baseline)return;
        for(size_t i=0;i<inputs.size();++i)results[i].ignored=!shouldMeasure(inputs[i],includeRelevantReleases);
    }
    void completeFinalValidationWarning(std::string why) {
        if (stage != Stage::FinalBaseline || !allMeasured()) {
            stage = Stage::Failed;
            error = std::move(why);
            return;
        }
        finalValidationWarning = true;
        finalValidationMessage =
            "Final reference validation failed after every input window was measured. "
            "Measurements were preserved, but the saved results are UNVERIFIED and COULD be inaccurate. " + why;
        stage = Stage::Done;
    }
    void fail(std::string why) {
        if(stage==Stage::Done && finalValidationWarning && allMeasured()) {
            finalValidationMessage+=" "+why;return;
        }
        if (stage == Stage::FinalBaseline && allMeasured()) {
            completeFinalValidationWarning(std::move(why));
            return;
        }
        stage = Stage::Failed; error = std::move(why);
    }
    void accept(bool success,int failureTick=-1) {
        ++trials;
        if (baseline()) {
            if (!success) {
                if (stage == Stage::FinalBaseline)
                    return completeFinalValidationWarning("The unshifted final replay did not reproduce the reference endpoint.");
                return fail("Reference replay desynced. No new measurements were accepted.");
            }
            if (stage == Stage::FinalBaseline || results.empty()) { stage = Stage::Done; return; }
            index=0;skipIgnored();
            if(index>=results.size()){stage=Stage::FinalBaseline;offset=0;return;}
            stage = Stage::Early; offset = -1; return;
        }
        if (finished() || index>=results.size()) return;
        auto& w = results[index];
        if (stage == Stage::Early) {
            if(!success)w.earlyFailureTick=failureTick;
            if (success) ++w.early;
            if (success && -offset < maxWindow) { --offset; return; }
            w.earlyCensored=success;
            stage = Stage::Late; offset = 1; return;
        }
        if(!success)w.lateFailureTick=failureTick;
        if (success) ++w.late;
        if (success && offset < maxWindow) { ++offset; return; }
        w.lateCensored=success;
        w.overflow = w.width() > maxWindow;
        advance();
    }
private:
    void skipIgnored() { while(index<results.size() && results[index].ignored)++index; }
    void advance() {
        results[index].measured = true;
        ++index;skipIgnored();
        if (index >= results.size()) { stage = Stage::FinalBaseline; offset = 0; }
        else { stage = Stage::Early; offset = -1; }
    }
};

struct Counts {
    std::array<int, maxWindow + 1> exact{};
    int above = 0, unknown = 0, presses = 0, releases = 0;
};
inline Counts count(Run const& r, int throughTick, bool releases, int playerFilter = 0) {
    Counts out;
    for (size_t i = 0; i < r.inputs.size(); ++i) {
        auto const& in = r.inputs[i];
        if (in.tick > throughTick || (!releases && !in.down) || (playerFilter && in.player + 1 != playerFilter)) continue;
        if(i<r.windows.size() && r.windows[i].ignored)continue;
        if(!shouldMeasure(in,releases))continue;
        in.down ? ++out.presses : ++out.releases;
        if (i >= r.windows.size() || !r.windows[i].measured) { ++out.unknown; continue; }
        auto const& w = r.windows[i];
        if (w.overflow || w.width() > maxWindow) ++out.above;
        else ++out.exact[w.width()];
    }
    return out;
}

// Symmetric Gaussian BASE precision estimate (NaNDL documents this model): independent symmetric normal input errors, no
// nerve/fatigue modifiers. Reverse recurrence avoids multiplying tiny success
// probabilities across thousands of inputs. Censored widths use their measured
// lower bound, giving an upper-bound estimate of the required precision.
inline double expectedSeconds(Run const& r, double precision, int throughTick) {
    double duration = static_cast<double>(std::min(throughTick, r.endTick)) / tickRate;
    double logExpected = std::log(std::max(duration, 1e-12));
    double logInverseTail = 0;
    for (size_t i = r.inputs.size(); i-- > 0;) {
        if (r.inputs[i].tick > throughTick) continue;
        if (i < r.windows.size() && r.windows[i].ignored) continue;
        if (i >= r.windows.size() || !r.windows[i].measured) return std::numeric_limits<double>::infinity();
        double t = r.inputs[i].tick / tickRate;
        double p = std::erf(r.windows[i].width() / tickRate * precision / (2.0 * std::sqrt(2.0)));
        if (!(p > 0)) return std::numeric_limits<double>::infinity();
        // E[completion] = T + sum(t_i * (1-p_i) / product(p_i..p_n)).
        logInverseTail -= std::log(p);
        if (t > 0 && p < 1) {
            double term = std::log(t) + std::log1p(-p) + logInverseTail;
            double hi = std::max(logExpected, term);
            logExpected = hi + std::log1p(std::exp(std::min(logExpected, term) - hi));
        }
    }
    return std::exp(logExpected);
}
inline std::optional<double> precision(Run const& r, int throughTick) {
    if(r.uncertain || r.finalValidationWarning)return {};
    if(r.nextClick)return {}; // Next-click successes do not imply whole-run survival.
    bool any = false;
    for (size_t i = 0; i < r.inputs.size(); ++i) {
        if (r.inputs[i].tick > throughTick) break;
        if (i < r.windows.size() && r.windows[i].ignored) continue;
        any = true;
        if (i >= r.windows.size() || !r.windows[i].measured) return {};
    }
    if (!any || throughTick <= 0 || throughTick / tickRate >= 86400.0) return {};
    double low = 0, high = 1;
    while (expectedSeconds(r, high, throughTick) > 86400.0 && high < 1e7) high *= 2;
    for (int n = 0; n < 70; ++n) {
        double mid = (low + high) / 2;
        if (expectedSeconds(r, mid, throughTick) > 86400.0) low = mid;
        else high = mid;
    }
    return high;
}
inline uint64_t hash(std::string const& text) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : text) { h ^= c; h *= 1099511628211ull; }
    return h;
}
}
