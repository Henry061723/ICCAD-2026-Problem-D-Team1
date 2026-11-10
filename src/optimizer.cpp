#include "stage2.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

SearchStats optimize(Design& d,const Evaluation& base,const Options& o,Clock::time_point start) {
    SearchStats st;

// 找出能驅動一個 child，且面積最小的 buffer。
int repairCell = -1;
for (int c = 0; c < static_cast<int>(d.cells.size()); ++c) {
    const auto& cell = d.cells[c];
    // 略過沒有 delay 數據（無法使用）的 cell
    if (cell.delay[0].empty() || cell.delay[1].empty())
        continue;
    // 比較面積 (width * height)，找出最小的
    if (repairCell < 0 ||
        cell.width * cell.height <
        d.cells[repairCell].width * d.cells[repairCell].height) {
        repairCell = c;
    }
}

// 先修復原始結構中 Root 直接連到 FF 的情況。
const int originalSize = static_cast<int>(d.tree.nodes.size());

for (int id = 1; id < originalSize; ++id) {
    if (d.tree.nodes[id].kind == Kind::Sink &&
        d.tree.nodes[id].parent == 0) {

        if (repairCell < 0) {
            throw std::runtime_error("no buffer available for repair");
        }

        if (st.inserted >= o.maxInsertions) {
            throw std::runtime_error("insertion limit prevents repair");
        }

        insertBuffer(d, id, repairCell);
        ++st.inserted;
    }
}

// 基準仍是未修改的原始測資；搜尋起點則是修復後的結構。
Evaluation current = evaluate(d, o.fixedArea);
double best = score(base, current, o).value;
    auto expired=[&]() {
        if(st.trials>=o.maxTrials) {st.stop="trial limit";return true;}
        if(std::chrono::duration<double>(Clock::now()-start).count()>=o.seconds) {st.stop="time budget";return true;}
        return false;
    };
    auto improve=[&](const Evaluation& e) {
        double value=score(base,e,o).value;
        if(preservesCleanCorners(base,e) && std::isfinite(value) && value>best+1e-12) {
            current=e; best=value;return true;
        }
        return false;
    };
    for(int pass=0;pass<o.passes;++pass) {
        bool changed=false;
        // Stable node IDs; accepted insertions append, never reorder nodes.
        const int size=static_cast<int>(d.tree.nodes.size());
        for(int id=1;id<size;++id) {
            if(d.tree.nodes[id].kind!=Kind::Buffer) continue;
            for(int c=0;c<static_cast<int>(d.cells.size());++c) {
                if(expired()) return st;
                int old=d.tree.nodes[id].cell;
                std::size_t fanout=d.tree.nodes[id].children.size();
                if(c==old || fanout>d.cells[c].delay[0].size() || fanout>d.cells[c].delay[1].size()) continue;
                resizeBuffer(d,id,c); ++st.trials;
                auto e=evaluate(d,o.fixedArea);
                if(improve(e)) {++st.resized;changed=true;}
                else resizeBuffer(d,id,old);
            }
        }
        // Prioritize endpoints involved in violations, but also consider subtree edges.
        auto a=clockArrival(d); std::vector<int> edges; std::vector<bool> seen(d.tree.nodes.size(),false);
        auto add=[&](int id) {if(id>0&&!seen[id]) {seen[id]=true;edges.push_back(id);}};
        for(int k=0;k<2;++k) for(const auto& p:d.reports[k].paths) {
            double skew=a[k][p.capture]-a[k][p.launch];
            double slack=k==0 ? .92*d.reports[k].period-p.delay+skew : p.delay-.05*d.reports[k].period-skew;
            if(slack<0) add(k==0?p.capture:p.launch);
        }
        for(int id=1;id<size;++id) add(id);
        for(int child:edges) {
            if(st.inserted>=o.maxInsertions) break;
            for(int c=0;c<static_cast<int>(d.cells.size());++c) {
                if(expired()) return st;
                int inserted=insertBuffer(d,child,c); ++st.trials;
                auto e=evaluate(d,o.fixedArea);
                if(improve(e)) {++st.inserted;changed=true;break;}
                undoInsertion(d,inserted);
            }
        }
        if(!changed) return st;
    }
    st.stop="pass limit";return st;
}
