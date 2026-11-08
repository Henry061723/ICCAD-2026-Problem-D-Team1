#include "stage2.hpp"
#include <regex>
#include <stdexcept>
#include <unordered_set>

void checkLegal(const Design& original,const Design& d) {
    validate(d);
    for (const auto& n : d.tree.nodes) {
    if (n.kind == Kind::Sink &&
        d.tree.nodes.at(n.parent).kind != Kind::Buffer) {
        throw std::runtime_error(
            "output SINK must be driven by a buffer: " + n.name);
    }
}
    const auto& old=original.tree; const auto& t=d.tree;
    if(t.nodes[0].name!=old.nodes[0].name) throw std::runtime_error("root renamed");
    std::unordered_set<std::string> names, added;
    std::regex pattern("NEW_BUF_(0|[1-9][0-9]*)");
    for(std::size_t i=0;i<t.nodes.size();++i) {
        const auto& n=t.nodes[i];
        if(!names.insert(n.name).second) throw std::runtime_error("duplicate name");
        auto index=t.byName.find(n.name);
        if(index==t.byName.end() || index->second!=static_cast<int>(i)) throw std::runtime_error("name index mismatch");
        if(i!=0 && n.kind==Kind::Root) throw std::runtime_error("multiple roots");
        if(n.kind==Kind::Buffer && n.type!=d.cells.at(n.cell).name) throw std::runtime_error("cell/type mismatch");
        if(!old.byName.count(n.name)) {
            if(n.kind!=Kind::Buffer || !std::regex_match(n.name,pattern)) throw std::runtime_error("invalid new buffer name: "+n.name);
            added.insert(n.name);
        }
    }
    if(t.byName.size()!=t.nodes.size()) throw std::runtime_error("extra name index entries");
    // New suffixes occupy the first available numbers, allowing existing names.
    std::size_t suffix=0;
    for(std::size_t i=0;i<added.size();++i) {
        while(old.byName.count("NEW_BUF_"+std::to_string(suffix))) ++suffix;
        if(!added.count("NEW_BUF_"+std::to_string(suffix++))) throw std::runtime_error("new buffer numbering gap");
    }
    // Contract new buffers: each original node must retain its original parent.
    // Single traversal avoids quadratic ancestor walks on deep trees.
    std::vector<std::pair<int,std::string>> todo{{0,""}};
    std::vector<std::string> actualOrder;
    while(!todo.empty()) {
        auto [id,ancestor]=todo.back(); todo.pop_back(); const auto& n=t.nodes[id];
        auto it=old.byName.find(n.name);
        if(it!=old.byName.end()) {
            const auto& before=old.nodes[it->second];
            if(n.kind!=before.kind || (n.kind==Kind::Sink && n.type!=before.type)) throw std::runtime_error("original component changed kind/type");
            const std::string expected=before.parent<0 ? "" : old.nodes[before.parent].name;
            if(ancestor!=expected) throw std::runtime_error("original ancestry changed: "+n.name);
            actualOrder.push_back(n.name); ancestor=n.name;
        }
        for(auto i=n.children.rbegin();i!=n.children.rend();++i) todo.emplace_back(*i,ancestor);
    }
    std::vector<std::string> expectedOrder; std::vector<int> stack{0};
    while(!stack.empty()) {
        int id=stack.back();stack.pop_back();const auto& n=old.nodes[id];expectedOrder.push_back(n.name);
        for(auto i=n.children.rbegin();i!=n.children.rend();++i) stack.push_back(*i);
    }
    if(actualOrder!=expectedOrder) throw std::runtime_error("original nodes deleted, renamed, or reordered");
}
