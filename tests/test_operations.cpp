#include "stage2.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <functional>
#include <stdexcept>
void rejects(const std::function<void()>& f) {
    bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);
}
int main(int argc,char** argv) {
    assert(argc==2); Design base=loadDesign(argv[1]); Design d=base;
    int child=d.tree.byName.at("FF_5"); int parent=d.tree.nodes[child].parent;
    auto originalChildren=d.tree.nodes[parent].children;
    int id=insertBuffer(d,child,0);
    assert(d.tree.nodes[id].name=="NEW_BUF_0");
    assert(d.tree.nodes[child].parent==id);
    checkLegal(base,d);
    undoInsertion(d,id);
    assert(d.tree.nodes[parent].children==originalChildren);
    assert(d.tree.nodes.size()==base.tree.nodes.size());
    checkLegal(base,d);
    resizeBuffer(d,parent,0);checkLegal(base,d);
    d=base;std::swap(d.tree.nodes[2].children[0],d.tree.nodes[2].children[1]);
    rejects([&]{checkLegal(base,d);});
    d=base;d.tree.nodes[child].name="RENAMED";
    rejects([&]{checkLegal(base,d);});
    d=base;id=insertBuffer(d,child,0);d.tree.byName.erase("NEW_BUF_0");d.tree.nodes[id].name="BAD";d.tree.byName["BAD"]=id;
    rejects([&]{checkLegal(base,d);});
    d=base; d.cells[0].delay[1].resize(1);
    rejects([&]{resizeBuffer(d,parent,0);});
    // Hand-computed score: .4*(.5+.5)+.3*(.25+.25)+.3*(.2)=.61.
    Evaluation b,n; b.metrics[0]={-10,-4,2};b.metrics[1]={-8,-2,2};b.area=100;
    n.metrics[0]={-5,-2,1};n.metrics[1]={-6,-1.5,2};n.area=80;
    auto s=score(b,n,Options{});assert(s.specDefined && std::abs(s.value-.61)<1e-12);
    b.metrics[0]={};n.metrics[0]={};s=score(b,n,Options{});assert(!s.specDefined);
    n.metrics[0].tns=-.1;assert(!preservesCleanCorners(b,n));
    // Insertion-only timing improvement; equal cell sizes prevent resize benefit.
    Design x;BufferCell c;c.name="B";c.width=c.height=1;c.delay[0]={.02,.02};c.delay[1]={.01,.01};x.cells={c};x.cellByName["B"]=0;
    Node root;root.name="ROOT";root.children={1};
    Node buf;buf.name="BUF";buf.type="B";buf.kind=Kind::Buffer;buf.parent=0;buf.children={2,3};buf.cell=0;
    Node a;a.name="A";a.type="FIFO";a.kind=Kind::Sink;a.parent=1;
    Node z=a;z.name="Z";x.tree.nodes={root,buf,a,z};
    for(int i=0;i<4;++i)x.tree.byName[x.tree.nodes[i].name]=i;
    x.reports[0].period=x.reports[1].period=1;
    x.reports[0].paths={{"p",2,3,.93}};x.reports[1].paths={{"p",2,3,.10}};
    auto initial=x; auto e=evaluate(x); Options o;o.maxInsertions=2;
    auto st=optimize(x,e,o,Clock::now());checkLegal(initial,x);
    assert(st.inserted>0 && score(e,evaluate(x),o).value>0);
    x=initial; x.reports[1].paths={{"p",3,2,.04}};
    auto bothOriginal=x; auto bothBase=evaluate(x);
    optimize(x,bothBase,o,Clock::now()); checkLegal(bothOriginal,x);
    auto bothScore=score(bothBase,evaluate(x),o);
    assert(bothScore.specDefined && bothScore.value>0);
    std::cout<<"PASS: operations, rollback, legality, score oracle, insertion optimization\n";
}
