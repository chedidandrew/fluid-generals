// Opt-in bounded battle profiling. GPL-3.0-or-later with EA terms.
#pragma once
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <map>
#include <algorithm>
#include "Common/NameKeyGenerator.h"
namespace cnc240 {
inline bool performanceProbeEnabled() {
    static const bool on=std::getenv("CNC240_VERIFY_PERFORMANCE") && std::getenv("CNC240_VERIFY_SECONDS") && std::getenv("CNC240_VERIFY_INPUT_ISOLATED");
    return on;
}
inline long long performanceStamp() { LARGE_INTEGER n;QueryPerformanceCounter(&n);return n.QuadPart; }
inline double performanceMicros(long long n) {
    static const double scale=[](){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return 1000000.0/f.QuadPart;}();return n*scale;
}
struct PerformanceSamples {
    std::vector<double> samples; double total=0,peak=0;
    void add(double us){samples.push_back(us);total+=us;peak=std::max(peak,us);}
};
struct BattlePerformance {
    std::map<const char*,PerformanceSamples> phases;
    std::map<int,PerformanceSamples> modules;
    unsigned long long pathHash=1469598103934665603ULL,pathPops=0,sortVisits=0,sortCalls=0;
    unsigned long long movementHits=0,movementMisses=0,movementChecks=0;
    unsigned stage=0,steps=0,maxSteps=0,paths=0,cells=0,queuePeak=0;
    double maxDebt=0;DWORD start=GetTickCount(),last=start;
    void report(unsigned frame) {
        if(!performanceProbeEnabled() || GetTickCount()-last<1000)return;
        const DWORD now=GetTickCount();
        printf("CNC240_PERF_WINDOW wall_ms=%lu window_ms=%lu frame=%u stage=%u steps=%u max_steps=%u debt_ms=%.3f paths=%u cells=%u queue_peak=%u\n",now-start,now-last,frame,stage,steps,maxSteps,maxDebt,paths,cells,queuePeak);
        printf("CNC240_PERF_PATHSORT wall_ms=%lu visits=%llu calls=%llu pops=%llu hash=%016llX\n",now-start,sortVisits,sortCalls,pathPops,pathHash);sortVisits=sortCalls=0;
        printf("CNC240_PERF_MOVEMENT_CACHE wall_ms=%lu hits=%llu misses=%llu rechecks=%llu\n",now-start,movementHits,movementMisses,movementChecks);movementHits=movementMisses=movementChecks=0;
        for(auto &p:phases) {
            auto &s=p.second;if(s.samples.empty())continue;std::sort(s.samples.begin(),s.samples.end());
            printf("CNC240_PERF_PHASE wall_ms=%lu stage=%u name=%s count=%u total_us=%.3f max_us=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f\n",now-start,stage,p.first,(unsigned)s.samples.size(),s.total,s.peak,s.samples[(s.samples.size()-1)/2],s.samples[(s.samples.size()-1)*95/100],s.samples[(s.samples.size()-1)*99/100]);
        }
        for(auto &p:modules) {
            auto &s=p.second;
            printf("CNC240_PERF_MODULE wall_ms=%lu stage=%u name=%s count=%u total_us=%.3f max_us=%.3f\n",now-start,stage,TheNameKeyGenerator->keyToName(static_cast<NameKeyType>(p.first)).str(),(unsigned)s.samples.size(),s.total,s.peak);
        }
        phases.clear();modules.clear();steps=maxSteps=paths=cells=queuePeak=0;maxDebt=0;last=now;fflush(stdout);
    }
};
inline BattlePerformance &battlePerformance(){static BattlePerformance p;return p;}
class PerformanceScope {
    const char *name;int module;long long start;
public:
    PerformanceScope(const char *n,int m=0):name(n),module(m),start(performanceProbeEnabled()?performanceStamp():0){}
    ~PerformanceScope(){if(start){auto &p=battlePerformance();double us=performanceMicros(performanceStamp()-start);if(module)p.modules[module].add(us);else p.phases[name].add(us);}}
};
inline int profileModule(int key) {
    static const bool enabled=std::getenv("CNC240_VERIFY_MODULE_PROFILE")!=nullptr;
    return performanceProbeEnabled() && enabled ? key:0;
}
}
