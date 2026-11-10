# ICCAD 2026 Problem D：第一次專題報告與實作

依據：使用者提供 D_20260525.pdf（2026-05-25 revision）。官網 https://www.iccad-contest.org/tw/03_problems.html 。本專案是教學 baseline，沒有最佳化演算法，也尚未經官方 testcase / checker 驗證。

## 1. 題目在解決什麼問題

題名 Timing Fixing by Useful Skew，出題單位 Realtek。輸入是一棵 clock tree、一份 buffer library，以及兩個 corner 的 data path delay report。允許插入 buffer 和替換既有 buffer 的型態（resizing），藉此改變各 flip-flop 收到 clock 的時間。資料路徑 delay 固定，不做邏輯修改、placement 或 routing。

Clock tree 是「clock source → buffers → FF clock pins」的樹；data path 是「launch FF → 組合邏輯 → capture FF」的有向關係。同一個 FF 可以出现在許多 data paths，這些資料路徑不構成 clock tree 的父子關係。檔案已提供資料路徑 delay，因此不必重建其中的邏輯閘。

令 L、C 分別為 launch、capture FF 的 clock arrival，skew=C-L。對同一 corner，arrival 是 root 到 FF 上所有 buffer delay 的和。buffer delay 依「目前直接 children 數量」查表；fanout 不是下游所有 FF 的數量。root 本身不查 buffer library、不額外加入 delay。

Setup slack = Tclk - Tsetup - Ddata + skew，其中 Tsetup=0.08Tclk。
Hold slack = Ddata - Thold - skew，其中 Thold=0.05Tclk。

Slack < 0 表示 violation。Capture clock 晚到，在其他數值固定時，setup margin 變大、hold margin 變小。延後一個 FF，也可能讓它作為 launch 的其他路徑 setup 變差。所以不能只修一條路徑。

SS（Slow-Slow）corner 計算 setup；FF（Fast-Fast）corner 計算 hold。「FF corner」不是「flip-flop 節點」。兩者 buffer delay、data delay 都需使用各自 corner 的資料，不能拿 SS skew 去算 FF hold。

程式採 signed convention：TNS=sum(min(slack,0))，WNS=min(0,所有 slack)。皆越接近 0 越好；沒有路徑或沒有 violation 時回傳 0。官方 checker 的輸出若採違規量絕對值，顯示符號會不同，須對照。程式未設定任意截斷 epsilon。

## 2. 操作與評分

允許 buffer insertion / resizing；不得刪除或改名既有元件；既有元件前後關係須保存。新增名稱 NEW_BUF_0、NEW_BUF_1……且全域唯一；每個 buffer 單一輸入，每個 FF 單一 buffer 驅動，pin 不得空接，各 corner fanout 上限均需符合。

例如 A→B→F，可改 A→NEW_BUF_0→B→F，不能改成 B→A→F。

Score = α[(1-TNS_SS/TNS_SS_ori)+(1-WNS_SS/WNS_SS_ori)] + β[(1-TNS_FF/TNS_FF_ori)+(1-WNS_FF/WNS_FF_ori)] + γ(1-Area/Area_ori)。附件給 0<α,β,γ<1 且總和=1，但沒有給具體值。增加面積會降低面積項。每組時限 10 分鐘，包含 I/O；違反格式或操作限制不予計分；總分同分再比總執行時間。

本 baseline 刻意不輸出 Score：附件沒有說原始 TNS/WNS 為零時如何處理除零；也未提供 FIFO/FF 面積。程式只計算已知 buffer area，不能把它直接宣稱為官方 Area。必須取得 Q&A/checker 後確認具體權重、面積定義與除零規則。

## 3. 檔案與設計模型

| 輸入 | 內容 | 模型 |
|---|---|---|
| buf.lib | SIZE、SS_DELAY、FF_DELAY | vector<BufferCell> |
| clk_tree.structure | Root、[depth] name (type) (SINK) | ClockTree |
| SS_delay.rpt | period、launch、capture、delay | reports[0] |
| FF_delay.rpt | period、launch、capture、delay | reports[1] |

輸出 modified_clk_tree.structure，格式與輸入 tree 相同。

BufferCell 存 name,width,height,delay[2]。例如 SS_DELAY 0.051 0.062 0.085 0.108，代表 fanout 1,2,3,4 對應延遲，SS 最大 fanout=4。vector 索引從 0 開始，所以查 delay[corner][fanout-1]。SS、FF 表長度分開保存，不預設相等。

Node 存 name、type、kind、parent ID、cell ID、children IDs。Root ID=0，parent=-1；sink 的 cell=-1，不能拿去查 buffer library。以 (SINK) 辨識端點，不靠 FF_ 開頭猜測。保留 FIFO 字串，輸出時不擅改型態。

vector<Node> 是節點的儲存空間，unordered_map<string,int> 是名字查 ID 的索引。ID 是 vector 下標，不是 node name 裡的編號。因此 BUF_81 不代表第 81 個節點。vector 擴充時 raw pointer/reference 可能失效，整數 ID 仍能索引原節點；故使用 ID。後續請勿刪除或重新排序 nodes，否則必須重建所有 ID。

DataPath 存 path name、launch ID、capture ID、data delay。每個 corner 分開存，避免用行號對齊。跨 corner 名稱不同、順序不同不會造成誤配。同一 corner 重複 path name 拒收；多條不同 path 可以有相同 endpoints，不擅自去重。

## 4. 專案架構

| 檔案 | 職責 |
|---|---|
| include/model.hpp | 所有資料模型與函式宣告 |
| src/parser.cpp | 檔案 → Design；含語法與引用解析 |
| src/analysis.cpp | validate、clock arrival、TNS/WNS、buffer area |
| src/writer.cpp | 迭代 preorder DFS 輸出 tree |
| src/main.cpp | CLI、協調流程、印摘要、例外處理 |
| examples/pdf_demo/ | 4 個教學輸入檔 |
| tests/test_cli.py | 錯誤輸入、手算 timing、round-trip 測試 |

main → loadDesign → validate → clockArrival → timing → writeTree。

目前不額外建立空的 optimizer.cpp；真正開發 insertion / resizing 時再新增此模組。Parser 不應負責最佳化，writer 不應計算 delay。這種分工讓未來換演算法時不必重寫檔案處理。

## 5. 一步一步讀懂 parser

### 第一步：共用輸入處理

readLines 開啟檔案、保留原始行號、移除首行 UTF-8 BOM、trim 空白、跳過空白行。CRLF 與 tabs 可接受。出錯訊息是 file:line:reason。report 的 # 開頭行與全 '-' 分隔線會跳過；tree/library 不任意略過未知行，防止錯誤被吞掉。沒有承諾支援附件以外的註解、inline comment 或任意換行語法。

number 用 stod 並確認整個 token 都被消耗；拒絕 nan、inf、負數、尾隨垃圾。時脈週期、尺寸必須大於 0；data/buffer delay 可以是 0。

### 第二步：先解析 buf.lib

使用簡單狀態機：區塊外必須遇到 cell (TYPE) {；區塊內收 SIZE、SS_DELAY、FF_DELAY；遇到 } 才把 cell 放進 Design。拒絕同名 cell、重複欄位、缺欄位、空 delay table、沒關閉的大括號。保留兩個 corner 不同的 delay 表長度。

選擇先讀 library，是為了讀 tree 時立即將 type 解析成 cell ID；未知型態可以立刻報錯。

### 第三步：以 stack 解析樹

stack[d] 的意義是目前走訪分支在深度 d 的 node ID。初始 stack=[root]。读到深度 depth 的新節點時：

1. depth 必須 >=1 且 <=stack.size()，禁止跳過父層。
2. stack.resize(depth)，保留 0 到 depth-1 的祖先。
3. parent=stack.back()。
4. 建立新節點，加入 parent.children。
5. stack.push_back(newID)。

例如 root → [1] BUF_0 → [2] BUF_4 → [3] FF_37，stack 依序是 [root]、[root,BUF_0]、[root,BUF_0,BUF_4]、[root,BUF_0,BUF_4,FF_37]。下一行 [3] FF_12 會先 resize(3)，把 FF_37 移出 stack，所以 FF_12 正確接到 BUF_4，不會接到 FF_37。

注意：stack 移出節點不代表從 tree 刪除節點。它只是保存目前路徑的暫存容器。

### 第四步：解析 report

先讀 Clock Period，再逐行讀 pathName : launch -> capture delay。用 tree.byName 查出兩個 ID，檢查都是 SINK；未知 FF 或 buffer endpoint 拒收。缺 period、重複 period、未知語法也拒收。

### 第五步：檢查語意

validate 以走訪確認全樹可從 root 到達，沒有重訪（cycle / multi-drive），parent/children 一致、SINK 無 child 且 parent 為 buffer、root/buffer 有 child、各 buffer fanout 在兩 corner 均合法。

這是初始結構 validator，並不是完整的「優化前後等價性 checker」。目前沒有修改樹，所以既有順序與命名自然保持；未來 optimizer 必須加上原樹對照與 NEW_BUF 命名檢查，不能只呼叫目前 validate 就當作官方合法。

### 第六步：求 arrival

a[node] 定義為 node 輸入端 clock arrival。root 的 arrival=0。處理 buffer 時，以 children.size() 查出自身 delay；對每個 child 指定 a[child]=a[node]+delay。SINK 的 a 值就是公式需要的 Dclk。使用 stack 的迭代 DFS，不依賴 parent ID 一定小於 child ID；將來新增 buffer 到 nodes 尾端也可正確走訪。

### 第七步：輸出並 round-trip

writer 先輸出 Root，再 preorder DFS。因 stack 後進先出，children 要反向 push，才能保持原輸入的 sibling 順序。depth 根據當前結構重算，不使用過期的輸入 depth。輸出仍保留元件名、型態、SINK 標記。

round-trip=讀 A → 寫 B → 讀 B → 寫 C，比較 B/C 完全相同並確認 timing 摘要相同。這能檢查 serializer 與 parser 一致，但不等同官方 checker。

## 6. 手算示範與已知答案

examples/pdf_demo 的 tree/library/SS report 取自附件例子；FF report 是自訂教學值，並特意交換 path 順序測試獨立解析。不能說這是官方 testcase。

| 元件 | 直接 fanout | SS delay | FF delay | buffer area |
|---|---:|---:|---:|---:|
| BUF_0 X8 | 3 | .032 | .0152 | 1.00 |
| BUF_4 X4 | 2 | .041 | .016 | .45 |
| BUF_81 X8 | 2 | .022 | .0098 | 1.00 |
| BUF_51 X2 | 1 | .051 | .020 | .25 |

總 buffer area=2.70。

| Sink | SS arrival | FF arrival |
|---|---:|---:|
| FF_37 / FF_12 | .073 | .0312 |
| FF_47 | .105 | .0450 |
| FF_5 | .054 | .0250 |
| FF_24 | .032 | .0152 |

SS T=.3，所以 setup=.024。Path1（FF_5→FF_12）skew=.019，slack=.3-.024-.162+.019=.133。Path2 skew=.032，slack=.3-.024-.207+.032=.101。兩條都沒有 setup violation。

自訂 FF T=.3，hold=.015。Path1 data=.018，skew=.0062，hold slack=-.0032。Path2 data=.010，skew=.0138，hold slack=-.0188。因此 FF TNS=-.022，WNS=-.0188，violations=2。

數值沿用檔案時間單位；附件範例未明示，不能擅自宣稱是 ns。

## 7. 在 Ubuntu 編譯與執行

解壓後進入 iccad_d_starter 資料夾。需要支援 C++17 的 g++；測試需要 Python 3。本專案不需要第三方 C++ library。

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp src/parser.cpp src/analysis.cpp src/writer.cpp -o cadd0000
./cadd0000 "$PWD/examples/pdf_demo" "$PWD/modified_clk_tree.structure"
python3 tests/test_cli.py ./cadd0000
```

實測編譯成功、無編譯警告，18 組整合測試通過（含 timing 手算與 round-trip）。沒有隨包附平台限定執行檔，請在你的 Linux 重編。cadd0000 是 placeholder，正式繳交改為隊伍編號。

預期摘要：

```text
buffers=4 sinks=5 buffer_area=2.700000000
SS_setup paths=2 TNS=0.000000000 WNS=0.000000000 violations=0
FF_hold paths=2 TNS=-0.022000000 WNS=-0.018800000 violations=2
Baseline tree written; no optimization performed.
```

另附 CMakeLists.txt，可選用：

```bash
cmake -S . -B build
cmake --build build -j2
./build/cadd0000 "$PWD/examples/pdf_demo" "$PWD/modified_clk_tree.structure"
```

本次環境無 cmake，所以驗證用上方 g++ 直接編譯方式；CMake 路徑未實跑。輸出檔的父資料夾必須已存在，且不可指定為任何輸入檔。CLI 成功 exit=0，解析/IO錯誤 exit=1，參數數量錯誤 exit=2。

## 8. 第一次報告的建議順序（約 8–10 分鐘）

1. 題目與目標：固定 data delay，用 buffer insertion/resizing 調 clock arrival；setup、hold、area 要一起考量。
2. 輸入輸出：展示四個輸入檔；指出 tree 與 data path 不同。
3. 資料結構：展示 model.hpp；說明 vector、name-to-ID map、parent/children 與兩 corner 表。
4. 程式架構：介紹五個程式檔責任及 main 流程。
5. Parser：以兩個同層 SINK 演示 stack resize；展示 library 狀態機及 endpoint 解析。
6. 合法性：fanout、重複名、缺父層、未知 FF 等錯誤檢查；不要說已完成官方 checker。
7. Demo：執行編譯、baseline、tests，對照手算結果與輸出檔。
8. 下一步：取得官方 testcase/checker/Q&A，補齊規格，再做單一步 insertion/resizing 的評估。

你可以這樣總結進度：「目前完成資料結構、三類輸入 parser、初始結構檢查、兩 corner timing 計算，以及相同結構的輸出。18 組自訂測試通過；尚未加入最佳化或跑官方 checker。」

## 9. 複雜度與下一階段

若 N 是節點數、P 是兩 corner 路徑總數、L 是 library 延遲表總元素數、B 是輸入總字元數：名字雜湊平均 O(1) 查找；解析通常近似 O(B)，但 std::regex/hash 不保證所有惡意输入的線性最壞界。驗證與 clock propagation O(N)，timing O(P)，空間 O(N+P+L) 加上暫存輸入文字。readLines 現在會整份讀入，若正式測資很大，可改為逐行串流以降低峰值記憶體。

writer 的成本按實際輸出字元數計算；四個空白乘深度的縮排，在退化長鏈可能達 O(N^2)，不能宣稱永遠 O(N)。迭代走訪避免 C++ call stack 溢位，但不消除長縮排的 I/O 成本。

最佳化下一步先提供受控 edit API：insertBuffer(parent, subsetChildren, cellId)、resizeBuffer(nodeId, cellId)，修改後 validate、全量重算、比較、可回復；先確認正確，再做 subtree arrival 更新與 incident path 索引加速。

保留 original tree 快照。對候選樹，把新增 buffer 跳過後，各既有節點的最近既有祖先應與原樹 parent 一致，以檢查插入所需的拓撲保存；再核對既有元件名/種類、FF 型態不变、新增名連續且唯一。實際合法操作邊界仍需依 Q&A/checker 確認。

不要假設 X8 一定比 X4 快；以當下 fanout 的 library 數值判斷。若插入 buffer 合併數個 child，parent fanout 會改變，parent delay 也要重算，可能影響該 parent 的所有下游 FF。此時只在一條 path 上加常數不是可靠更新方式。

官方網站本次可讀到題目 06/03、Q&A 07/21、testcase 09/04、checker 06/15 更新標記；Google Drive 連結未提供可讀文件內容，故未合併其未知補充。下一輪請提供 Q&A PDF、testcase ZIP、checker 使用說明，以便驗證格式與評分規則。
