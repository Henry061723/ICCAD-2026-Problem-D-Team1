#include "stage2.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

Evaluation evaluate(const Design& d,double fixedArea) {
    auto arrivals=clockArrival(d); Evaluation e;
    for(int k=0;k<2;++k) e.metrics[k]=timing(d,k,arrivals[k]);
    e.area=bufferArea(d)+fixedArea;
    if(!std::isfinite(e.area) || e.area<=0) throw std::runtime_error("invalid total area");
    return e;
}
Score score(const Evaluation& base,const Evaluation& now,const Options& o) {
    Score s;
    auto gain=[&](double before,double after) {
        if(before==0) {
            s.specDefined=false;
            // Local policy only: unchanged clean timing contributes zero.
            // A newly introduced violation is forbidden by the optimizer.
            return after==0 ? 0.0 : -std::numeric_limits<double>::infinity();
        }
        return 1-after/before;
    };
    s.value=o.alpha*(gain(base.metrics[0].tns,now.metrics[0].tns)+gain(base.metrics[0].wns,now.metrics[0].wns))
        +o.beta*(gain(base.metrics[1].tns,now.metrics[1].tns)+gain(base.metrics[1].wns,now.metrics[1].wns))
        +o.gamma*(1-now.area/base.area);
    return s;
}
bool preservesCleanCorners(const Evaluation& base,const Evaluation& now) {
    for(int k=0;k<2;++k) if(base.metrics[k].tns==0 && now.metrics[k].tns<0) return false;
    return true;
}
void writeMetrics(const std::filesystem::path& file,const Evaluation& base,const Evaluation& now,
                  const Score& s,double elapsed,const SearchStats& st,const Options& o) {
    std::ofstream f(file); if(!f) throw std::runtime_error("cannot write metrics: "+file.string());
    f<<std::setprecision(17);
    f<<"phase,setup_wns,setup_tns,hold_wns,hold_tns,area,local_score,spec_score_defined,elapsed_seconds,legal,positive_score,alpha,beta,gamma,fixed_area,trials,resizes,insertions\n";
    auto row=[&](const char* name,const Evaluation& e,double value) {
        f<<name<<','<<e.metrics[0].wns<<','<<e.metrics[0].tns<<','<<e.metrics[1].wns<<','<<e.metrics[1].tns<<','
         <<e.area<<','<<value<<','<<s.specDefined<<','<<elapsed<<",1,"<<(value>0)<<','<<o.alpha<<','<<o.beta<<','<<o.gamma<<','<<o.fixedArea<<','<<st.trials<<','<<st.resized<<','<<st.inserted<<'\n';
    };
    row("original",base,0); row("optimized",now,s.value);
    f.close(); if(!f) throw std::runtime_error("metrics write failed");
}
