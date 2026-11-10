#include "stage2.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

double numeric(const std::string& s) {
    std::size_t end=0; double v=std::stod(s,&end);
    if(end!=s.size() || !std::isfinite(v) || v<0) throw std::runtime_error("invalid option number: "+s);
    return v;
}
int main(int argc,char** argv) {
    const auto start=Clock::now();
    if(argc<3) {
        std::cerr<<"Usage: cadd0000 CASE_DIR OUTPUT [--alpha .4 --beta .3 --gamma .3] [--seconds 540] [--max-trials 20000] [--passes 8] [--max-insertions 1000] [--fixed-area 0] [--no-opt] [--check MODIFIED_TREE]\n";
        return 2;
    }
    try {
        Options o; std::filesystem::path dir=argv[1],output=argv[2],check;
        for(int i=3;i<argc;++i) {
            std::string key=argv[i];
            if(key=="--no-opt") {o.optimize=false;continue;}
            if(i+1==argc) throw std::runtime_error("missing option value: "+key);
            std::string value=argv[++i];
            if(key=="--check") {check=value;continue;}
            double v=numeric(value);
            if(key=="--alpha") o.alpha=v;
            else if(key=="--beta") o.beta=v;
            else if(key=="--gamma") o.gamma=v;
            else if(key=="--seconds") o.seconds=v;
            else if(key=="--fixed-area") o.fixedArea=v;
            else if(key=="--max-trials" || key=="--max-insertions" || key=="--passes") {
                if(v!=std::floor(v) || v>1000000000) throw std::runtime_error("invalid integer limit");
                if(key=="--passes") o.passes=static_cast<int>(v);
                else if(key=="--max-trials") o.maxTrials=static_cast<std::size_t>(v);
                else o.maxInsertions=static_cast<std::size_t>(v);
            } else throw std::runtime_error("unknown option: "+key);
        }
        if(o.alpha<=0 || o.beta<=0 || o.gamma<=0 || o.alpha>=1 || o.beta>=1 || o.gamma>=1 || std::abs(o.alpha+o.beta+o.gamma-1)>1e-9) throw std::runtime_error("weights must be positive, below 1, and sum to 1");
        if(o.seconds>570) throw std::runtime_error("seconds must be <=570 to reserve I/O time");
        const std::filesystem::path csv=output.string()+".metrics.csv";
        for(const auto* name:{"buf.lib","clk_tree.structure","SS_delay.rpt","FF_delay.rpt"})
            for(const auto& dest:{output,csv})
                if(std::filesystem::weakly_canonical(dir/name)==std::filesystem::weakly_canonical(dest) ||
                   (std::filesystem::exists(dest)&&std::filesystem::equivalent(dir/name,dest)))
                    throw std::runtime_error("output must not overwrite an input file");
        Design original=loadDesign(dir); auto base=evaluate(original,o.fixedArea);
        Design d=check.empty()?original:loadDesignWithTree(dir,check);
        SearchStats stats;
        if(!check.empty()) stats.stop="check only";
        else if(o.optimize) stats=optimize(d,base,o,start); // 使用到 optimize()
        else stats.stop="optimization disabled";
        checkLegal(original,d);
        auto now=evaluate(d,o.fixedArea); auto s=score(base,now,o);
        if(check.empty()) writeTree(d.tree,output);
        double elapsed=std::chrono::duration<double>(Clock::now()-start).count();
        if(check.empty()) writeMetrics(csv,base,now,s,elapsed,stats,o);
        elapsed=std::chrono::duration<double>(Clock::now()-start).count();
        std::cout<<std::fixed<<std::setprecision(9);
        std::cout<<"phase         Setup_WNS      Setup_TNS      Hold_WNS       Hold_TNS       Area\n";
        auto print=[&](const char* phase,const Evaluation& e) {
            std::cout<<std::left<<std::setw(14)<<phase<<std::setw(15)<<e.metrics[0].wns<<std::setw(15)<<e.metrics[0].tns<<std::setw(15)<<e.metrics[1].wns<<std::setw(15)<<e.metrics[1].tns<<e.area<<'\n';
        };
        print("original",base); print("optimized",now);
        std::cout<<"Score="<<s.value<<" spec_score_defined="<<s.specDefined<<" legal=1 positive_score="<<(s.value>0)<<'\n';
        std::cout<<"elapsed_seconds="<<elapsed<<" trials="<<stats.trials<<" resizes="<<stats.resized<<" insertions="<<stats.inserted<<" stop="<<stats.stop<<'\n';
        std::cout<<"Area = buffer area + fixed_area ("<<o.fixedArea<<").\n";
        if(!s.specDefined) std::cout<<"NOTICE: zero original timing denominator; Score uses LOCAL zero policy, not a defined SPEC score.\n";
        if(s.value<=0) std::cout<<"NOTICE: legal result but positive-score goal NOT met.\n";
        if(elapsed>=600) {std::cerr<<"ERROR: 600-second limit exceeded\n";return 3;}
    } catch(const std::exception& e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
