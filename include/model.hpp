#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// ==========================================
// 1. 元件庫模組 (對應輸入檔：buf.lib)
// ==========================================
struct BufferCell {
    std::string name;
    double width=0, height=0;
    std::array<std::vector<double>,2> delay; // index = fanout - 1
    // SS corner 索引 = 0 用於 setup；
    // FF corner 索引 = 1 用於 hold。
};

// ==========================================
// 2. 時脈樹模組 (對應輸入檔：clk_tree.structure)
// ==========================================
enum class Kind { Root, Buffer, Sink }; // 對應clock source、buffer、ff的clock的接收端
struct Node {
    std::string name, type;
    Kind kind = Kind::Root;
    int parent = -1; // 父節點在 nodes 中的索引
    std::vector<int> children; // 保存輸入順序；ID 不隨 vector 擴充而失效
    int cell = -1; // design.cells 裡的buffer型號資料
};
struct ClockTree {
    std::vector<Node> nodes; // root ID = 0
    std::unordered_map<std::string, int> byName;// 節點名稱查出節點索引
};

// ==========================================
// 3. 時序分析模組 (對應輸入檔：SS_delay.rpt, FF_delay.rpt)
// ==========================================
struct DataPath {
    std::string name;
    int launch=-1, capture=-1;// ff 在 nodes 中的索引
    double delay=0;
};
struct Report { 
    double period=0; 
    std::vector<DataPath> paths; 
};// corner的週期 & data路徑清單

// ==========================================
// 4. 頂層設計圖 (將上述資料整合，代表整顆晶片)
// ==========================================
struct Design {
    std::vector<BufferCell> cells;// 存放各種buffer，每個元素型別都是BufferCell
    std::unordered_map<std::string, int> cellByName;// Buffer名稱對應在cells裡的位置。
    ClockTree tree;
    std::array<Report, 2> reports; // SS、FF corner 分開存兩份報告
};


struct Metrics { double tns=0, wns=0; std::size_t violations=0; };
Design loadDesign(const std::filesystem::path& directory);
void validate(const Design& d);
std::array<std::vector<double>,2> clockArrival(const Design& d);
Metrics timing(const Design& d, int corner, const std::vector<double>& arrival);
double bufferArea(const Design& d);
void writeTree(const ClockTree& t, const std::filesystem::path& output);
