# ICCAD 2026 Problem D：第二階段

這份程式從你 2026-11-06 上傳的第一階段成果延伸，保留 C++17、原本資料結構、Parser 與延遲計算方式。
建議先閱讀本檔；README_zh-TW.md 是第一階段歷史教材，其中「尚未最佳化」等敘述不代表這一版。

## 1. 助教的三項要求與本版功能

| 報告要求 | 本版提供 |
|---|---|
| 原始與修改後 Setup WNS/TNS（SS）、Hold WNS/TNS（FF）、Area | 終端機表格與每組 metrics.csv |
| SPEC Score，權重可調 | --alpha / --beta / --gamma；預設 0.4 / 0.3 / 0.3 |
| 插入 buffer | 在 parent→child 邊上插入單輸出 buffer，保留 sibling 位置 |
| 更換尺寸 | 更換 library cell，更新 Node.cell 及 Node.type，先檢查兩個 corner fanout |
| 輸出 | 指定路徑的 modified_clk_tree.structure |
| 檢查器 | 檢查結構、fanout、名稱、原始元件保留、原始祖先與兄弟順序、NEW_BUF_X 編號 |
| 第一版 optimizer | 有時間及候選數上限的 greedy local search，僅接受分數改善 |
| 五組測試統計 | run_all.py 自動發現 testcase，輸出 summary.csv、逐組 log；預期組數預設 5 |
| 執行時間 | C++ 計時及 Python 完整程序 wall time；批次工具 timeout=600 秒 |

**尚未取得及執行助教／官方五組 testcase，不能保證這五組分數皆大於零。**
正分是驗收條件；若搜尋不到改善，本版輸出合法原樹並明確顯示 positive_score=0，不捏造分數。

## 2. 評分與規格上的待確認事項

依 D_20260525.pdf 第 2、6 節：

- skew = capture clock arrival − launch clock arrival。
- SS setup slack = 0.92 × SS period − SS data delay + SS skew。
- FF hold slack = FF data delay − 0.05 × FF period − FF skew。
- TNS = 所有負 slack 的總和；WNS = min(0, 所有 slack)。均以負值表示違規。
- Score = α[(1−TNS_SS/TNS_SS_ori)+(1−WNS_SS/WNS_SS_ori)]
  + β[(1−TNS_FF/TNS_FF_ori)+(1−WNS_FF/WNS_FF_ori)]
  + γ(1−Area/Area_ori)。**TNS/WNS 兩項不再除以 2。**
- 0<α,β,γ<1，且三者和為 1；CLI 會檢查。

### 零分母

目前取得的 SPEC 沒有指定原始 WNS/TNS=0 的處理方式。不能用任意 epsilon 代替並聲稱是官方分數。
本版在零分母時標記 `spec_score_defined=0`，顯示的 Score / CSV local_score 是本地擴充規則：
原本零違規、修改後仍零違規，該項貢獻為 0；若新增違規則視為負無限分並拒絕。
因此原樹的本地分數仍為 0。所有分母非零時，local_score 就等於上面的 SPEC 公式。
批次程式把正分但零分母的案例標為 LOCAL_SCORE_ONLY，不能當作官方正分驗收通過。
後續拿到官方 checker／Q&A 後，應核對並視需要調整 scoring.cpp。

### 面積

buf.lib 只給 buffer 的寬高，沒有給 SINK/FF 面積。本版 Area=buffer 面積總和+`--fixed-area`，預設 fixed-area=0，終端會明示。
若助教／checker 的 Area 包含固定 FF 等元件總面積，請用 `--fixed-area` 傳入該總和；不同 testcase 可個別執行設定。
固定面積雖然不變，但會影響正規化分母，不能聲稱它會完全抵銷。
在核對面積定義前，分數應視為「採用目前面積假設的結果」。

## 3. 檔案架構

- include/model.hpp：沿用第一階段的 Design、Node、BufferCell、Report。
- include/stage2.hpp：Options、Evaluation、Score、SearchStats 與新增 API。
- src/parser.cpp：沿用解析流程；新增 loadDesignWithTree 供外部輸出檢查。
- src/analysis.cpp：原有合法樹檢查、arrival、slack/TNS/WNS、buffer area。
- src/scoring.cpp：前後評估、Score、CSV。
- src/operations.cpp：resizeBuffer、insertBuffer、undoInsertion。
- src/legality.cpp：相對於原始設計的合法性檢查。
- src/optimizer.cpp：第一版 greedy optimizer。
- src/writer.cpp：輸出樹；採固定縮排、以 [depth] 表示深度，避免深樹縮排產生平方級輸出。
- src/main.cpp：參數、讀檔、最佳化、檢查、輸出與統計。
- scripts/run_all.py：多組測試、600 秒限制、輸出重新解析、summary.csv。
- tests/：第一階段 18 項回歸測試，以及新增的操作／評分／optimizer 測試。
- *.reference：保存第一階段主程式與原測試作為教材，不參與編譯。

## 4. Optimizer 怎麼運作

1. 載入設計，保存 original 與原始時序／面積。所有分數都使用這份固定 baseline 作分母。
2. 依序替各 buffer 嘗試其他 library 型號，排除兩 corner fanout 不足者。
3. 每個候選重新計算整棵樹 arrival、所有資料路徑 slack、WNS/TNS、面積與分數。
4. 分數提升超過 1e-12 且不破壞原始 clean corner 才接受，否則復原。
5. 優先在 setup 違規 capture、hold 違規 launch 前試插入，再試其他邊；每個新增 buffer fanout=1。
6. 插入成功保留；失敗立即撤回，NEW_BUF 編號不會因失敗候選留下缺號。
7. 重複數輪，沒有改善或達到輪數／候選數／時間限制便停止。
8. 檢查完整輸出相對原樹是否合法，再寫檔。

Resize 同時更改型號名稱與 cell ID；插入同時更新 parent.children、child.parent、nodes、byName。
vector 擴充時不可繼續使用舊的 Node 參考，因此插入後重新透過 ID 存取。
撤回只能針對最新的試插入節點，不提供刪除原始元件的操作。
對新增 buffer 的移除只用於拒絕候選；本版沒有已接受插入的任意刪除／重新分群策略。

檢查器會在遍歷修改樹時忽略新增 buffer，確認每個原始節點的最近原始祖先仍等於原 parent，
並比較忽略新增節點後的 preorder，保守地保留既有 sibling 順序。
外部 `--check` 以原始 buf.lib 與 reports 搭配修改樹重新解析，避免沿用記憶體 ID 而漏掉輸出錯誤。

## 5. 編譯（Ubuntu / Linux）

在本資料夾開啟終端機：

```bash
make
```

或不使用 make：

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude src/*.cpp -o cadd0000
```

亦提供 CMakeLists.txt：

```bash
cmake -S . -B build
cmake --build build -j4
```

不要直接沿用第一階段只列四個 cpp 的編譯指令，否則會漏掉新增模組。

## 6. 下一步測試時使用的命令

你目前要求先完成實作；這裡先準備好測試入口，正式五組驗證留到下一步。

### 小型範例

```bash
mkdir -p results/demo
./cadd0000 examples/pdf_demo results/demo/modified_clk_tree.structure
```

輸出檔旁還會產生 `modified_clk_tree.structure.metrics.csv`。
這個 demo 原始 setup=0，所以一定會看到 zero denominator 提醒；它不是官方 testcase。

### 可調參數

```bash
./cadd0000 examples/pdf_demo results/demo/modified_clk_tree.structure --alpha 0.4 --beta 0.3 --gamma 0.3 --seconds 540 --max-trials 20000 --passes 8 --max-insertions 1000
```

`--seconds` 是從程式啟動算起的搜尋截止時間，預設 540、最大 570，保留讀寫與檢查餘裕。
截止只在候選之間檢查，單次完整評估或解析無法被中斷；不是硬性 600 秒保證。
批次工具用 subprocess timeout 強制 600 秒，上限包括讀檔、搜尋、寫檔與程序退出。
CSV 的 elapsed_seconds 是寫完 tree 後的時間，略早於 CSV 自身寫完；**報告用 summary.csv 的 wall_seconds**。
外部重讀檢查的時間另計，不混入 optimizer 的 wall_seconds。

### 僅輸出 baseline / 外部合法性檢查

```bash
./cadd0000 examples/pdf_demo results/demo/baseline.structure --no-opt
./cadd0000 examples/pdf_demo results/demo/unused.structure --check results/demo/modified_clk_tree.structure
```

`--check` 不寫輸出，第二個位置參數仍須提供作為 CLI 介面占位；結果列印到終端機。

### 自訂回歸測試

```bash
make test
```

### 正式五組

假設 `official_cases/` 底下有五個 testcase 資料夾，每個有四個輸入檔：

```bash
python3 scripts/run_all.py ./cadd0000 /你的路徑/official_cases ./results/official --expected-count 5
```

結果位於 `results/official/summary.csv` 與各組子資料夾。輸出資料夾不得位於輸入資料夾內。
每組 log 會列原始與修改後時序／面積、分數、操作次數與停止原因。
狀態 OK 代表本地檢查通過、公式分數為正且未逾時；仍須交由官方 checker 最終確認。
NONPOSITIVE / LOCAL_SCORE_ONLY / TIMEOUT / CHECK_FAILED 都不能當作五組驗收成功。

## 7. 已完成的開發驗證

- GCC C++17、-Wall -Wextra -Wpedantic 編譯。
- 第一階段 18 項測試：合法輸入、手算時序、往返輸出、重複／缺漏／格式錯誤等。
- 分數手算 oracle（.61）、零分母政策、參數驗證與固定面積。
- 插入、撤回、resize 與非法名稱／順序／fanout 拒絕。
- 特製 insertion-only 範例：確認實際接受插入並提高本地分數。
- demo 最佳化後重新讀取輸出，再以原始設計檢查。

這些屬於實作時的小型驗證，不等於官方五組效能測試。

## 8. 複雜度與下次改善方向

目前每個候選全量重算，約 O(N+P) 時間與 O(N+P) 工作記憶體，N 為節點、P 為路徑。
總時間約 O(候選數 × (N+P))；插入命名也需搜尋已使用編號。
這是容易檢查正確性的基礎版，沒有保證全域最佳或所有 testcase 正分。
後續可加入受影響子樹 arrival 更新、FF→data paths 反向索引、候選優先序、增量 WNS/TNS，
以及兩步／多步聯合操作，改善大型測試時間與局部最佳問題。
每次比較應同時看兩 corner、Area、Score 與執行時間。

## 9. Git 管理

交付包含 `history.bundle`，保存「上傳的第一階段」與「第二階段」兩個版本。
可從 bundle 還原歷史：

```bash
git clone history.bundle iccad_d_with_history
cd iccad_d_with_history
git log --oneline
```

若只想從解壓後目錄開始自己的 git：

```bash
git init
git add .
git commit -m "Stage 2 baseline optimizer"
```

每次修改前後先 `git status`，一個可運作的小功能完成就 commit；官方大測資與產出請放在 repo 外。
