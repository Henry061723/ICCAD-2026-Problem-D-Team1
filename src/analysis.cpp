#include "model.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

void validate(const Design& d) {
    const auto& nodes=d.tree.nodes;
    if(nodes.empty() || nodes[0].kind!=Kind::Root || nodes[0].parent!=-1) throw std::runtime_error("invalid root");
    std::vector<bool> seen(nodes.size(),false); std::vector<int> pending{0};
    while(!pending.empty()) {
        int id=pending.back(); pending.pop_back();
        if(id<0 || static_cast<std::size_t>(id)>=nodes.size() || seen[id]) throw std::runtime_error("cycle, multi-drive, or invalid node ID");
        seen[id]=true; const auto& n=nodes[id];
        if(n.kind==Kind::Sink) {
            if (!n.children.empty() ||
    n.parent < 0 ||
    static_cast<std::size_t>(n.parent) >= nodes.size() ||
    (nodes[n.parent].kind != Kind::Buffer &&
     nodes[n.parent].kind != Kind::Root)) {
    throw std::runtime_error("invalid SINK: " + n.name);
}
        } else if(n.children.empty()) throw std::runtime_error("unconnected output: "+n.name);
        if(n.kind==Kind::Buffer) {
            if(n.cell<0 || static_cast<std::size_t>(n.cell)>=d.cells.size()) throw std::runtime_error("invalid cell ID");
            const auto& c=d.cells[n.cell];
            for(int k=0;k<2;++k) if(n.children.size()>c.delay[k].size()) throw std::runtime_error("fanout exceeds corner "+std::to_string(k)+" limit: "+n.name);
        }
        for(int child:n.children) {
            if(child<0 || static_cast<std::size_t>(child)>=nodes.size() || nodes[child].parent!=id) throw std::runtime_error("parent/child mismatch");
            pending.push_back(child);
        }
    }
    if(std::find(seen.begin(),seen.end(),false)!=seen.end()) throw std::runtime_error("unreachable nodes");
}
std::array<std::vector<double>,2> clockArrival(const Design& d) {
    std::array<std::vector<double>,2> a;
    for(int k=0;k<2;++k) {
        a[k].assign(d.tree.nodes.size(),0); std::vector<int> stack{0};
        while(!stack.empty()) {
            int id=stack.back(); stack.pop_back(); const auto& n=d.tree.nodes[id];
            double delay=0;
            if(n.kind==Kind::Buffer) delay=d.cells[n.cell].delay[k].at(n.children.size()-1);
            // a[id] 是該元件輸入端的 arrival。Buffer delay 加到每個 child。
            double next=a[k][id]+delay;
            if(!std::isfinite(next)) throw std::runtime_error("clock arrival overflow");
            for(int child:n.children) { a[k][child]=next; stack.push_back(child); }
        }
    }
    return a;
}
Metrics timing(const Design& d,int k,const std::vector<double>& a) {
    Metrics m; const auto& r=d.reports[k];
    for(const auto& p:r.paths) {
        double skew=a.at(p.capture)-a.at(p.launch);
        double slack=k==0 ? 0.92*r.period-p.delay+skew : p.delay-0.05*r.period-skew;
        if(!std::isfinite(slack)) throw std::runtime_error("slack overflow");
        if(slack<0) { m.tns+=slack; m.wns=std::min(m.wns,slack); ++m.violations; }
    }
    if(!std::isfinite(m.tns)) throw std::runtime_error("TNS overflow");
    return m;
}
double bufferArea(const Design& d) {
    double total=0;
    for(const auto& n:d.tree.nodes) if(n.kind==Kind::Buffer) { const auto& c=d.cells[n.cell]; total+=c.width*c.height; }
    if(!std::isfinite(total)) throw std::runtime_error("area overflow");
    return total;
}
