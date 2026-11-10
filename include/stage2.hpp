#pragma once
#include "model.hpp"
#include <chrono>
#include <limits>

using Clock = std::chrono::steady_clock;
struct Options {
    double alpha=.4, beta=.3, gamma=.3;
    double seconds=540, fixedArea=0;
    std::size_t maxTrials=20000, maxInsertions=1000;
    int passes=8;
    bool optimize=true;
};
struct Evaluation {
    std::array<Metrics,2> metrics;
    double area=0;
};
struct Score {
    double value=0;
    bool specDefined=true;
};
struct SearchStats {
    std::size_t trials=0, resized=0, inserted=0;
    std::string stop="local optimum";
};
Design loadDesignWithTree(const std::filesystem::path&,const std::filesystem::path&);
Evaluation evaluate(const Design&,double fixedArea=0);
Score score(const Evaluation& original,const Evaluation& current,const Options&);
bool preservesCleanCorners(const Evaluation&,const Evaluation&);
void resizeBuffer(Design&,int node,int cell);
int insertBuffer(Design&,int child,int cell);
void undoInsertion(Design&,int newId);
void checkLegal(const Design& original,const Design& modified);
SearchStats optimize(Design&,const Evaluation&,const Options&,Clock::time_point start);
void writeMetrics(const std::filesystem::path&,const Evaluation&,const Evaluation&,const Score&,double,const SearchStats&,const Options&);
