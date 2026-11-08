#include "model.hpp"
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {
using Lines=std::vector<std::pair<int,std::string>>;// pair < int 行號 , string該行內容 >
// 移除文字前空白
std::string trim(const std::string& s) {		
    auto a=s.find_first_not_of(" \t\r\n");// 找出第一個不是空白的位置
    return a==std::string::npos ? "" : s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);// 若找不到->回傳空字串，找到->取出第一個到最後一個非空白字元之間的文字
}
[[noreturn]] void fail(const std::filesystem::path& p,int line,const std::string& why) {
    throw std::runtime_error(p.string()+":"+std::to_string(line)+": "+why);
}
Lines readLines(const std::filesystem::path& p) {
    std::ifstream in(p); if(!in) fail(p,0,"cannot open file");
    Lines out; std::string s; int n=0;
    while(std::getline(in,s)) {
        ++n;
        if(n==1 && s.compare(0,3,"\xEF\xBB\xBF")==0) s.erase(0,3);
        s=trim(s); if(!s.empty()) out.emplace_back(n,s);
    }
    if(in.bad()) fail(p,n,"read failure");
    return out;
}

// 將字串轉為數值，檢查是否有效
double number(const std::string& s,
              const std::filesystem::path& p,
              int line,
              bool allowNegative = false) {
    std::size_t end = 0;
    double v = 0;

    try {
        v = std::stod(s, &end);
    } catch (...) {
        fail(p, line, "invalid number: " + s);
    }

    if (end != s.size() || !std::isfinite(v)) {
        fail(p, line, "expected finite number: " + s);
    }

    if (!allowNegative && v < 0) {
        fail(p, line, "expected nonnegative number: " + s);
    }

    return v;
}

// 解析buf.lib
void library(Design& d,const std::filesystem::path& p) {							
    const std::regex start(R"(^cell\s*\(\s*([^\s()]+)\s*\)\s*\{\s*$)");
    bool active=false; BufferCell cell; std::unordered_set<std::string> fields;
    for(const auto& [line,s]:readLines(p)) {
        std::smatch m;
        if(!active) {
            if(!std::regex_match(s,m,start)) fail(p,line,"expected cell (TYPE) {");
            cell=BufferCell{}; cell.name=m[1]; fields.clear(); active=true;
        } else if(s=="}") {
            if(fields.size()!=3) fail(p,line,"cell requires SIZE, SS_DELAY, FF_DELAY");
            if(d.cellByName.count(cell.name)) fail(p,line,"duplicate cell: "+cell.name);
            d.cellByName.emplace(cell.name,static_cast<int>(d.cells.size()));
            d.cells.push_back(cell); active=false;
        } else {
            std::istringstream row(s); std::string key; row>>key;
            if(!fields.insert(key).second) fail(p,line,"duplicate field: "+key);
            if(key=="SIZE") {
                std::string w,by,h,extra;
                if(!(row>>w>>by>>h) || by!="BY" || (row>>extra)) fail(p,line,"expected SIZE width BY height");
                cell.width=number(w,p,line); cell.height=number(h,p,line);
                if(cell.width==0 || cell.height==0 || !std::isfinite(cell.width*cell.height)) fail(p,line,"invalid cell area");
            } else if(key=="SS_DELAY" || key=="FF_DELAY") {
                auto& a=cell.delay[key=="SS_DELAY"?0:1]; std::string x;
                while(row>>x) a.push_back(number(x,p,line));
                if(a.empty()) fail(p,line,"empty delay table");
            } else fail(p,line,"unknown library field: "+key);
        }
    }
    if(active) fail(p,0,"unclosed cell block");
    if(d.cells.empty()) fail(p,0,"empty library");
}

// 解析clk_tree.structure，建立父子關係
void tree(Design& d,const std::filesystem::path& p) {								
    const std::regex root(R"(^Root\s*:\s*([^\s()]+)\s*$)");
    const std::regex node(R"(^\[([0-9]+)\]\s+([^\s()]+)\s+\(\s*([^\s()]+)\s*\)\s*(\(SINK\))?\s*$)");
    std::vector<int> stack;
    for(const auto& [line,s]:readLines(p)) {
        std::smatch m;
        if(d.tree.nodes.empty()) {
            if(!std::regex_match(s,m,root)) fail(p,line,"expected Root: NAME");
            Node n; n.name=m[1]; d.tree.nodes.push_back(n); d.tree.byName.emplace(n.name,0); stack.push_back(0);
            continue;
        }
        if(!std::regex_match(s,m,node)) fail(p,line,"invalid tree node");
        std::size_t depth=0;
        try { depth=std::stoull(m[1]); } catch(...) { fail(p,line,"invalid depth"); }
        if(depth==0 || depth>stack.size()) fail(p,line,"depth must start at 1 and cannot skip levels");
        stack.resize(depth);
        Node n; n.name=m[2]; n.type=m[3]; n.kind=m[4].matched?Kind::Sink:Kind::Buffer;
        n.parent=stack.back();
        if(d.tree.nodes[n.parent].kind==Kind::Sink) fail(p,line,"SINK cannot have children");
        if(d.tree.byName.count(n.name)) fail(p,line,"duplicate component: "+n.name);
        if(n.kind==Kind::Buffer) {
            auto it=d.cellByName.find(n.type);
            if(it==d.cellByName.end()) fail(p,line,"unknown buffer type: "+n.type);
            n.cell=it->second;
        }
        int id=static_cast<int>(d.tree.nodes.size());
        d.tree.byName.emplace(n.name,id); d.tree.nodes.push_back(n);
        d.tree.nodes[n.parent].children.push_back(id); stack.push_back(id);
    }
    if(d.tree.nodes.empty()) fail(p,0,"empty clock tree");
}

// 建立delay report
Report report(const Design& d,const std::filesystem::path& p) {
    const std::regex clock(R"(^Clock\s+Period\s*:\s*(\S+)\s*$)");
    const std::regex path(R"(^([^\s:]+)\s*:\s*([^\s]+)\s*->\s*([^\s]+)\s+(\S+)\s*$)");
    Report r; bool seen=false; std::unordered_set<std::string> names;
    for(const auto& [line,s]:readLines(p)) {
        if(s[0]=='#' || s.find_first_not_of('-')==std::string::npos) continue;
        std::smatch m;
        if(std::regex_match(s,m,clock)) {
            if(seen) fail(p,line,"duplicate Clock Period");
            r.period=number(m[1],p,line); seen=true;
            if(r.period==0) fail(p,line,"Clock Period must be positive");
            continue;
        }
        if(!seen || !std::regex_match(s,m,path)) fail(p,line,"invalid report row or missing Clock Period");
        DataPath x; x.name=m[1];
        if(!names.insert(x.name).second) fail(p,line,"duplicate path name: "+x.name);
        auto resolve=[&](const std::string& name) {
            auto it=d.tree.byName.find(name);
            if(it==d.tree.byName.end() || d.tree.nodes[it->second].kind!=Kind::Sink) fail(p,line,"endpoint is not a known SINK: "+name);
            return it->second;
        };
        x.launch=resolve(m[2]); x.capture=resolve(m[3]); x.delay=number(m[4],p,line,true);
        r.paths.push_back(x);
    }
    if(!seen) fail(p,0,"missing Clock Period");
    return r;
}
}

// 依序呼叫上述函式，建立完整設計資料
Design loadDesignWithTree(const std::filesystem::path& directory, const std::filesystem::path& structure) {// 接收測資資料夾的位置，const不修改變路徑
    Design d;
    library(d,directory/"buf.lib");
    tree(d,structure);
    d.reports[0]=report(d,directory/"SS_delay.rpt");
    d.reports[1]=report(d,directory/"FF_delay.rpt");
    validate(d); return d;
}

Design loadDesign(const std::filesystem::path& directory) {
    return loadDesignWithTree(directory,directory/"clk_tree.structure");
}
