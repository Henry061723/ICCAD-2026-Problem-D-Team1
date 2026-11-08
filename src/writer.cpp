#include "model.hpp"
#include <fstream>
#include <stdexcept>

void writeTree(const ClockTree& t,const std::filesystem::path& output) {
    std::ofstream out(output); if(!out) throw std::runtime_error("cannot write: "+output.string());
    out<<"Root: "<<t.nodes.at(0).name<<'\n';
    std::vector<std::pair<int,int>> stack;
    for(auto it=t.nodes[0].children.rbegin();it!=t.nodes[0].children.rend();++it) stack.emplace_back(*it,1);
    while(!stack.empty()) {
        auto [id,depth]=stack.back(); stack.pop_back(); const auto& n=t.nodes[id];
        // [depth] 是結構依據，縮排僅便於閱讀。迭代 DFS 避免深樹遞迴溢位。
        out<<"    "<<'['<<depth<<"] "<<n.name<<" ("<<n.type<<')';
        if(n.kind==Kind::Sink) out<<" (SINK)";
        out<<'\n';
        for(auto it=n.children.rbegin();it!=n.children.rend();++it) stack.emplace_back(*it,depth+1);
    }
    out.close(); if(!out) throw std::runtime_error("output write failed");
}
