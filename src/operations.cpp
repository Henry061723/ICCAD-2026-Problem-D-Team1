#include "stage2.hpp"
#include <algorithm>
#include <stdexcept>

void resizeBuffer(Design& d,int id,int cell) {
    auto& n=d.tree.nodes.at(id); const auto& c=d.cells.at(cell);
    if(n.kind!=Kind::Buffer) throw std::runtime_error("resize requires buffer");
    for(int k=0;k<2;++k) if(n.children.size()>c.delay[k].size()) throw std::runtime_error("resize fanout limit");
    n.cell=cell; n.type=c.name;
}
int insertBuffer(Design& d,int child,int cell) {
    if(child<=0) throw std::runtime_error("cannot insert above root");
    const auto& c=d.cells.at(cell);
    if(c.delay[0].empty() || c.delay[1].empty()) throw std::runtime_error("insertion fanout limit");
    int parent=d.tree.nodes.at(child).parent;
    auto& siblings=d.tree.nodes.at(parent).children;
    auto it=std::find(siblings.begin(),siblings.end(),child);
    if(it==siblings.end()) throw std::runtime_error("missing insertion edge");
    std::size_t slot=static_cast<std::size_t>(it-siblings.begin());
    std::size_t number=0;
    while(d.tree.byName.count("NEW_BUF_"+std::to_string(number))) ++number;
    Node n; n.name="NEW_BUF_"+std::to_string(number); n.kind=Kind::Buffer;
    n.type=c.name; n.cell=cell; n.parent=parent; n.children={child};
    int id=static_cast<int>(d.tree.nodes.size());
    d.tree.nodes.push_back(n); // Do not retain vector references across this push.
    d.tree.byName.emplace(n.name,id);
    d.tree.nodes[parent].children[slot]=id;
    d.tree.nodes[child].parent=id;
    return id;
}
void undoInsertion(Design& d,int id) {
    // Only undo the most recent trial insertion, never remove an original node.
    if(id!=static_cast<int>(d.tree.nodes.size())-1) throw std::runtime_error("undo must be LIFO");
    const Node n=d.tree.nodes.at(id);
    if(n.children.size()!=1) throw std::runtime_error("undo requires unary buffer");
    auto& siblings=d.tree.nodes.at(n.parent).children;
    auto it=std::find(siblings.begin(),siblings.end(),id);
    if(it==siblings.end()) throw std::runtime_error("missing undo edge");
    *it=n.children[0]; d.tree.nodes[n.children[0]].parent=n.parent;
    d.tree.byName.erase(n.name); d.tree.nodes.pop_back();
}
