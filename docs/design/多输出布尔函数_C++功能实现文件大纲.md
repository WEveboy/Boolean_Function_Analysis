# 多输出布尔函数安全性指标：C++ 功能实现文件大纲

依据张卫国《密码函数》第 3 章 3.1 节（书页 166–171）和同目录 XMind 导图设计，并补充 S 盒的线性近似表（LAT）分析。本文件是实现规划，不表示这些模块已经编写。范围为导入一个 \((n,m)\) 布尔函数，按需计算该节指标，以及并列的 S 盒差分分析和线性分析；构造方法属于 3.2 节及以后，不纳入此大纲。

## 1. 数据约定与指标边界

- `F : F₂ⁿ → F₂ᵐ`，内部存储 `2^n` 个输出值 `F(x)`，每个值恰有 `m` 位。输入索引按 `00…0` 到 `11…1` 升序；`x1` 和 `f1` 均作为最高位。此接口草案使用 32 位索引与输出字，先限制 `1≤n≤30`、`1≤m≤min(n,31)`，再由实际计算预算收紧。所有元数、长度、输出范围在导入时校验。
- 对每个非零输出掩码 `c` 形成分量 `f_c(x)=parity(c & F(x))`。分量型判据须覆盖全部 `2^m-1` 个 `c`。只检查坐标 `f1,…,fm` 不足以判定向量函数平衡、弹性、最小次数、AI 或非线性度。
- S 盒直接复用 `VectorFunction` 的查找表；允许一般 `n` 输入、`m` 输出，`n=m` 且输出互异时另标记为置换 S 盒。DDT 的行列分别是输入差分 `a`、输出差分 `b`；LAT 的行列分别是输入掩码 `a`、输出掩码 `b`，均按无符号数值升序排列。
- 平衡与弹性需要 `n≥m`；`t` 阶弹性还要求 `0≤t≤n−m`。差分均匀性定义中的平均值为 `2^(n−m)`，该版精确整数实现也限定 `n≥m`。超出范围应返回明确状态，不静默取整。
- 允许 `m=1` 用于验证与复用单输出算法，但对 `PN/APN/AB/多输出 bent` 的分类须按书中相应参数条件判定。空 `c`、零差分 `a=0` 不参加非零分量、差分指标扫描。
- 零多项式次数用内部哨值 `-1` 表示，并在结果里单独标注；这是实现约定，不把它误作书中的非负次数。对仿射退化函数的 EA 变换结果重新计算次数。

## 2. 建议文件树

```text
include/vbf/
  vector_function.hpp        (n,m) 函数、真值表、输入与置换性验证
  io.hpp                     多输出真值表 TXT/CSV 导入、可选转置与规范导出
  component.hpp              输出掩码、分量提取、遍历
  transforms.hpp             GF(2) Möbius、Walsh-Hadamard 变换
  balance.hpp                输出直方图、平衡判定
  resiliency.hpp             Walsh 判据、最大弹性阶
  degree.hpp                 最大/最小分量次数
  algebraic_immunity.hpp     向量代数免疫阶 AI 的精确求解
  nonlinearity.hpp           分量与向量非线性度
  spectral_class.hpp         几乎最优、多输出 bent 的轻量判定
  ddt.hpp                    仅按要求生成差分行与聚合数据
  lat.hpp                    仅按要求生成线性近似列与完整 LAT
  differential_uniformity.hpp 差分均匀度、PN/APN 判定
  differential_deviation.hpp 最大偏差与标准差
  linear_analysis.hpp        最大相关幅度、线性偏差与近似概率
  almost_bent.hpp            AB 复合判定，显式依赖 APN 与非线性度
  equivalence.hpp            给定仿射/EA 参数的变换；CCZ 图像校验
  analysis_session.hpp       按需缓存分量/ANF/Walsh，合并批量差分扫描
  result.hpp                 指标值、见证掩码、计算状态、错误
src/vbf/
  vector_function.cpp        与同名头文件逐一对应
  io.cpp
  component.cpp
  transforms.cpp
  balance.cpp
  resiliency.cpp
  degree.cpp
  algebraic_immunity.cpp
  nonlinearity.cpp
  spectral_class.cpp
  ddt.cpp
  lat.cpp
  differential_uniformity.cpp
  differential_deviation.cpp
  linear_analysis.cpp
  almost_bent.cpp
  equivalence.cpp
  analysis_session.cpp
src/main.cpp                  CLI 参数、按需调用、JSON/文本结果
tests/
  known_vectors.cpp          恒等置换、例 3.2、x³ 等已知向量
  identities.cpp             Walsh、直方图、DDT、LAT 交叉核验
  sbox_tables.cpp            小维 S 盒的 DDT/LAT 全表及约定测试
  lazy_dispatch.cpp          单指标无关模块不运行；批量共享公共中间量
  limits.cpp                 参数、溢出、范围状态
```

接口层采用 C++20。库函数不依赖 CLI 或界面；现有桌面程序若要接入，可在其进程协议层调用新 CLI 或独立库。每个指标有独立入口；`analysis_session` 只负责在批量请求中复用公共中间量，不触发未请求的指标。

## 3. 核心类型与调用接口

```cpp
namespace vbf {
using Input = std::uint32_t;
using Output = std::uint32_t;
using Count = std::uint64_t;
using SignedSpectrum = std::int64_t;

struct VectorFunction {
    unsigned n, m;
    std::vector<Output> truth; // truth[x] == F(x), 长度 2^n
    static VectorFunction from_outputs(unsigned n, unsigned m,
                                       std::vector<Output> values);
    Output at(Input x) const noexcept;
};
bool is_permutation(const VectorFunction&); // n=m 且输出恰好覆盖全部 m 位值

enum class Status { Exact, OutOfRange, NotApplicable };
template<class T> struct MetricResult {
    Status status;
    std::optional<T> value;
    std::string reason;
};

std::vector<std::uint8_t> component_truth(const VectorFunction&, Output c);
std::vector<Count> output_histogram(const VectorFunction&);
struct BalanceResult { bool balanced; Count expected; std::vector<Count> counts; };
MetricResult<BalanceResult> balance(const VectorFunction&);

using Walsh = std::vector<SignedSpectrum>;
Walsh component_walsh(const VectorFunction&, Output c);
struct ResiliencyResult { int order; Output failing_c; Input failing_a; };
MetricResult<bool> is_t_resilient(const VectorFunction&, unsigned t);
MetricResult<ResiliencyResult> resiliency_order(const VectorFunction&);

struct DegreeExtreme { int degree; Output witness_c; };
MetricResult<DegreeExtreme> maximum_degree(const VectorFunction&);
MetricResult<DegreeExtreme> minimum_degree(const VectorFunction&);
struct ImmunityResult { unsigned minimum; Output weakest_c; };
MetricResult<ImmunityResult> algebraic_immunity(const VectorFunction&,
                                                unsigned exact_max_n);

struct NonlinearityResult {
    Count minimum;
    Output weakest_c;
    Input peak_frequency;
};
MetricResult<NonlinearityResult> nonlinearity(const VectorFunction&);
MetricResult<bool> is_almost_optimal(const VectorFunction&, Count nonlinearity);
MetricResult<bool> is_strictly_almost_optimal(const VectorFunction&, Count nonlinearity);
MetricResult<bool> is_vectorial_bent(const VectorFunction&, Count nonlinearity);

using DdtRow = std::vector<Count>; // 固定一个 a，按 b 索引
DdtRow ddt_row(const VectorFunction&, Input a);
using DdtTable = std::vector<DdtRow>; // table[a][b]，包含 a=0 的核验行
MetricResult<DdtTable> differential_distribution_table(const VectorFunction&,
                                                        std::size_t max_cells);
struct DifferentialExtreme { Count value; Input witness_a; Output witness_b; };
MetricResult<DifferentialExtreme> differential_uniformity(const VectorFunction&);
MetricResult<long double> maximum_differential_probability(const VectorFunction&);
MetricResult<DifferentialExtreme> maximum_differential_deviation(const VectorFunction&);
MetricResult<long double> differential_standard_deviation(const VectorFunction&);
MetricResult<bool> is_perfect_nonlinear(const VectorFunction&, Count uniformity);
MetricResult<bool> is_apn(const VectorFunction&, Count uniformity);

using LatColumn = Walsh; // 固定输出掩码 b，列中按输入掩码 a 索引
LatColumn lat_column(const VectorFunction&, Output b);
using LatRow = std::vector<SignedSpectrum>;
using LatTable = std::vector<LatRow>; // table[a][b]，保留符号和零掩码
MetricResult<LatTable> linear_approximation_table(const VectorFunction&,
                                                   std::size_t max_cells);
struct LinearExtreme {
    SignedSpectrum coefficient; // 带符号的 LAT[a][b]
    Input input_mask;
    Output output_mask;
};
MetricResult<LinearExtreme> maximum_linear_correlation(const VectorFunction&);
MetricResult<long double> maximum_linear_bias(const VectorFunction&);
MetricResult<long double> maximum_linear_probability(const VectorFunction&);
MetricResult<bool> is_almost_bent(const VectorFunction&, Count uniformity,
                                  Count nonlinearity);
}
```

上面的指标入口可单独调用，也可通过 `AnalysisSession` 使用缓存。`is_almost_bent` 的参数必须由调用者显式提供，避免仅请求非线性度就计算 DDT。调用方可使用类型化结果保存见证与状态；计算状态不作为数值零返回。

大规模真值表不无条件展开 `2^n × 2^m` 的完整 DDT 或 LAT。`ddt_row` 一次构造一行；`lat_column` 一次取得一个输出掩码的全部输入掩码系数。单指标请求只维护所需极值或累加量；完整表必须显式请求并通过 `max_cells` 内存检查。多个差分指标可在同一次 DDT 行扫描中共享行计数；多个线性指标可复用同一个 LAT 列的 Walsh 谱。两组分析互不触发。

## 4. 文件职责与计算方法

| 文件 | 方法及结果 | 关键限制 |
|---|---|---|
| `vector_function.cpp` / `io.cpp` | 校验 `n,m`、长度与每个输出值；`is_permutation` 检查方阵 S 盒输出是否互异；支持逐词真值表和可选转置输入（`f1` 至 `fm` 各一行，每行按 `x=0…2^n−1` 排列），转置仅在导入阶段完成；规范导出逐词真值表。 | 转置二进制行须有 `2^n` 位，十六进制行须有 `⌈2^n/4⌉` 位并保留前导零；严格校验行数与位序。 |
| `component.cpp` | 非零 `c` 遍历、`parity(c & F(x))` 提取分量；统一位序。 | `m` 太大导致 `2^m−1` 分量不可枚举时返回 `OutOfRange`。 |
| `transforms.cpp` | 对分量真值表做 GF(2) 快速 Möbius 变换，取得 ANF；对 `(-1)^f` 做 FWHT，取得 `W_f(a)`。 | `W_f(0)=2^n−2wt(f)`，`Σ_a W_f(a)^2=2^(2n)` 可作自检；使用有符号 64 位并先检查 `n`。 |
| `balance.cpp` | `output_histogram` 与 `balance` 只扫描向量真值表，不求 ANF 或 Walsh。 | 书页 167 例 3.2 逐一核查全部非零分量；测试还应包含“坐标均平衡而整体不平衡”的反例。 |
| `resiliency.cpp` | `is_t_resilient(t)` / `resiliency_order` 仅取得所需的低重量 Walsh 系数；若已缓存完整谱则复用。 | `t≤n−m`，同时报告首个失败的 `(c,a)`；单独请求平衡不调用此模块。 |
| `degree.cpp` | `maximum_degree` 可仅用各坐标的 ANF；`minimum_degree` 扫描全部非零分量的 ANF，处理最高次项抵消。 | 不调用 AI 的零化子求解器。 |
| `algebraic_immunity.cpp` | `algebraic_immunity` 对每个 `f_c` 与 `1⊕f_c` 在递增次数下建立 GF(2) 零化子方程，取最小。 | AI 精确运算成本高，单独限制 `n` 和分量数；请求次数时不运行 AI。 |
| `nonlinearity.cpp` | `nonlinearity` 用各分量 Walsh 求 `N_F=min_c[2^(n−1)−max_a|W_{f_c}(a)|/2]`。 | 仅返回非线性度及最弱分量，不求 DDT 或 AB。 |
| `spectral_class.cpp` | 已有 `N_F` 时用参数与阈值判几乎最优、严格几乎最优、多输出 bent；这些函数不重新计算频谱。 | 分类调用在用户请求时进行；不因请求非线性度自动执行。 |
| `ddt.cpp` | `ddt_row(a)` 对固定 `a` 扫描 `x`，计数 `F(x⊕a)⊕F(x)`；完整 DDT 显式请求时才保留所有 `a` 行。 | `a=0` 行供展示与核验；安全指标仅扫描 `a≠0`。行和为 `2^n`，非零差分行各计数为偶数。 |
| `lat.cpp` | `lat_column(b)` 生成 `(-1)^(b·F(x))` 后做 FWHT，得到所有 `a` 的带符号 `LAT[a][b]`；完整 LAT 显式请求时才组装所有列。 | `b=0` 列供展示与核验；线性攻击指标使用 `a≠0,b≠0`，不把平凡项 `LAT[0][0]=2^n` 计入极值。 |
| `differential_uniformity.cpp` | `differential_uniformity` 只取各行最大值；`maximum_differential_probability` 把该值除以 `2^n`；`is_perfect_nonlinear`、`is_apn` 只做参数与已得 `δ_F` 的判定。 | 仅请求 `δ_F` 时不计算偏差或平方和；批量概率请求复用最大值。 |
| `differential_deviation.cpp` | `maximum_differential_deviation` 只追踪最大绝对偏差；`differential_standard_deviation` 只累加偏差平方和，最后除以 `2^m(2^n−1)` 并开方。 | 求平方和使用至少 128 位或受控高精度；两者不会因请求 `δ_F` 而执行。 |
| `linear_analysis.cpp` | `maximum_linear_correlation` 取 `a≠0,b≠0` 的最大 `|LAT[a][b]|` 并保留原符号及掩码；`maximum_linear_bias`、`maximum_linear_probability` 根据该幅度换算。 | 不扫描 DDT；保留有符号系数，避免把负偏差当作无效近似。 |
| `almost_bent.cpp` | `is_almost_bent` 在显式请求时组合 `δ_F` 与 `N_F`，再检查 `n=m` 和奇数 `n`。 | 若任一前置指标超范围，返回相应状态，不假定 `false`。 |
| `equivalence.cpp` | 接受已给定的 GF(2) 仿射输入置换、输出置换与 EA 附加仿射映射，生成新真值表；可按图像集合检验已给定的 CCZ 仿射映射。 | 不承诺搜索未知等价变换；验证置换矩阵可逆与图像一一对应。 |
| `analysis_session.cpp` | 先解析请求集合，再按依赖计算：同一分量 ANF/Walsh 可复用；多个差分请求共享逐行 DDT 计数；LAT 与非线性度可复用 Walsh 列。 | 设显式内存预算；DDT 请求不触发 LAT，LAT 请求不触发 DDT。 |
| `main.cpp` / `result.hpp` | `--input --n --m --metrics`，可选 `--transpose` 切换导入布局，显式 `--ddt`、`--lat` 导出全表；结构化输出指标值、状态、掩码见证及表的定义约定。 | 输入错误与不可计算状态区别报告；表格单元数超预算时报 `OutOfRange`。 |

## 5. S 盒差分分析与线性分析

两组功能是并列入口，共用 `VectorFunction` 的查找表，但各自生成不同的表与指标。S 盒直接以 `VectorFunction` 表示，不复制真值表。`is_permutation` 在 `n=m` 时检查输出互异；一般 `n→m` 查找表仍可计算 DDT、LAT。

### 5.1 差分分析：DDT

- **差分分布表**：`DDT[a][b]=#{x | F(x⊕a)⊕F(x)=b}`，共有 `2^n` 行、`2^m` 列。完整表包含 `a=0` 行，该行 `DDT[0][0]=2^n`、其余为 0；安全指标不使用这一平凡行。
- **单项与全表**：`ddt_row(a)` 只算指定输入差分；`differential_distribution_table(max_cells)` 显式生成完整表。扫描时用 `F(x⊕a)⊕F(x)` 作列索引并加一；计数为非负整数。
- **差分攻击指标**：`δ_F=max_{a≠0,b}DDT[a][b]`；最大差分概率 `DP_max=δ_F/2^n`，同时报告达到最大值的 `(a,b)`。PN/APN、最大偏差和标准差使用现有独立函数，不因请求 DDT 以外的其他指标而自动计算。
- **核验**：每行计数和为 `2^n`；`a≠0` 时 `DDT[a][b]` 为偶数。若完整表已请求，差分均匀度及偏差指标可直接复用表格；若只请求摘要，则逐行处理并释放。

### 5.2 线性分析：LAT

- **线性近似表约定**：本项目以完整的**带符号 Walsh 值**定义 `LAT[a][b]=Σ_x(-1)^(a·x⊕b·F(x))`，共有 `2^n` 行、`2^m` 列。`a` 是输入掩码，`b` 是输出掩码，点积为 GF(2) 位点积。某些软件把 LAT 显示为该值的一半；导入、导出和测试必须标明采用本项目的完整值约定。
- **单项与全表**：`lat_column(b)` 构造 `(-1)^(b·F(x))`，经一次 FWHT 得到此输出掩码下全部 `a` 的系数。`linear_approximation_table(max_cells)` 显式生成完整表。`b=0` 列作为核验数据保留：`LAT[0][0]=2^n`，`LAT[a≠0][0]=0`。对于平衡 S 盒，`LAT[0][b≠0]=0`。
- **近似命中与偏差**：等式 `a·x=b·F(x)` 的命中次数为 `(2^n+LAT[a][b])/2`，概率为 `1/2+LAT[a][b]/2^(n+1)`；带符号概率偏差为 `LAT[a][b]/2^(n+1)`。负系数表示互补等式更常成立，不能丢弃符号。
- **线性攻击指标**：扫描 `a≠0,b≠0`，取最大相关幅度 `L_max=max|LAT[a][b]|`、见证掩码 `(a,b)`、带符号系数；最大绝对概率偏差为 `L_max/2^(n+1)`，最佳等式或其互补式的命中概率为 `1/2+L_max/2^(n+1)`。`maximum_linear_correlation` 与 `maximum_linear_probability` 是两个独立请求；批量调用时共享极值扫描。
- **与非线性度的关系**：`N_F=2^(n−1)−(1/2)max_{b≠0,a≥0}|LAT[a][b]|`。这里非线性度包含 `a=0`，与线性攻击极值排除 `a=0` 的范围不同；仅当所有非零分量平衡时，两者的最大系数范围才可直接合并。
- **核验**：每个 `b` 的列满足 `Σ_a LAT[a][b]^2=2^(2n)`；每个系数在 `[-2^n,2^n]` 且与 `2^n` 同奇偶；`lat_column(b)` 应与已缓存的 `component_walsh(F,b)` 逐项相等。

### 5.3 表格输出与资源控制

- `--ddt` 和 `--lat` 分别请求 `DDT.csv`、`LAT.csv`；CSV 首行标出各列的 `b`，首列标出各行的 `a`，掩码使用固定宽度十六进制并说明位序。DDT 单元格为无符号计数，LAT 单元格为带符号的完整 Walsh 值；结果元数据写明 `n,m`、表类型及 LAT 数值约定。
- 输出全表前以安全的宽整数检查 `2^(n+m)` 个单元是否超过 `max_cells` 和文件大小预算；超过时返回 `OutOfRange`，仍允许只请求摘要指标。若同时请求两张表，分别检查并生成，单张表失败不伪装成另一张表的结果。
- 表格请求不顺带计算另一张表。已缓存的 Walsh 列可供 LAT 使用；若已请求完整 LAT，非线性度、弹性可从相关列复用。已请求完整 DDT 时，差分指标可直接复用各行。缓存只在同一输入实例内有效。
- CLI 示例：`vbf-analyze --input sbox.txt --n 4 --m 4 --metrics differential_uniformity,maximum_linear_correlation --ddt --lat --out results`。仅指定 `--ddt` 时不计算 LAT，仅指定 `--lat` 时不计算 DDT；全表 CSV 与指标摘要分别输出。

## 6. 按需调用与复用规则

| 用户请求 | 需要的计算 | 不触发的独立计算 |
|---|---|---|
| `balance` | 一次输出直方图扫描 | 分量生成、ANF、Walsh、AI、DDT |
| `maximum_degree` | 坐标 ANF 与最高次数 | 其余输出掩码、AI、Walsh、DDT |
| `minimum_degree` | 全部非零分量 ANF 与最低次数 | AI、Walsh、DDT |
| `algebraic_immunity` | 全部非零分量的零化子求解 | 次数、Walsh、DDT |
| `nonlinearity` | 全部非零分量 Walsh | AI、DDT、AB 分类 |
| `resiliency_order` | 所需 Walsh 系数；可复用已算谱 | AI、DDT、非线性度公式 |
| `differential_uniformity` | 逐行 DDT 计数及行最大值 | Walsh、AI、偏差、标准差 |
| `maximum_differential_probability` | `δ_F` 极值与除法；批量时复用 `δ_F` | LAT、Walsh、AI |
| `maximum_differential_deviation` | 逐行 DDT 计数及最大偏差 | Walsh、AI、平方和 |
| `differential_standard_deviation` | 逐行 DDT 计数及偏差平方和 | Walsh、AI、最大值比较 |
| `differential_distribution_table` / `--ddt` | 完整 DDT，含零差分行 | LAT、Walsh、AI |
| `linear_approximation_table` / `--lat` | 所有输出掩码的 Walsh 列，组装完整 LAT | DDT、AI、差分摘要公式 |
| `maximum_linear_correlation` | 非零输入与输出掩码的最大 `|LAT|`，无需存全表 | DDT、AI、差分指标 |
| `maximum_linear_bias` / `maximum_linear_probability` | 同一线性极值及概率换算，批量时复用极值 | DDT、AI、差分指标 |
| `is_almost_bent` | 按依赖请求 `δ_F` 和 `N_F` 后判定 | AI、次数、偏差、标准差 |

批量请求在调用前合并依赖。例如同时请求 `δ_F`、最大偏差和标准差，只扫描一次所有 DDT 行，并在行生成后把它送入三个已启用的聚合器；只请求其中一个时仅创建对应聚合器。同理，弹性、非线性度与 LAT 相关请求可复用分量 Walsh 谱。`balance` 直接用输出直方图，不能因为平衡也有 Walsh 判据就强制计算频谱。DDT 与 LAT 分属差分、线性两条依赖链，请求其中一条不会启动另一条。

`AnalysisSession` 持有同一输入的惰性缓存：`component(c)`、`anf(c)`、`walsh(c)` 仅在首次请求时计算。DDT 默认采用逐行流式处理；若一次请求已完成、之后又单独追加新的差分指标，允许重新扫描，避免为未知后续请求常驻完整 DDT。缓存键包含输入实例和分量掩码，换输入即失效。缓存预算不足时清退中间量，不改变结果。

请求调度示例：

```cpp
AnalysisSession session(F);
auto b = session.balance();                  // 只遍历 F(x)
auto d = session.minimum_degree();           // 首次构造所需分量 ANF
auto nl = session.nonlinearity();             // 首次构造所需 Walsh
auto ab = session.almost_bent();              // 复用 nl，另外扫描 DDT 求 δ_F
```

CLI 的 `--metrics` 明确指定请求集合；默认不执行 `--all`。测试应给每个底层模块加调用计数或可注入的计算探针，断言单指标请求未触发无关模块，并断言批量请求共享一次公共变换或 DDT 行扫描。

## 7. 分类公式与实现判定

1. **平衡 / 弹性**：`count[β]=2^(n−m)`；`t` 阶弹性要求所有非零 `c` 和 `wt(a)≤t` 的 Walsh 值为 0。`t=0` 与平衡的等价性作为内部交叉校验。
2. **非线性度**：取所有非零分量的最小非线性度。几乎最优的门槛在奇数 `n` 为 `2^(n−1)−2^((n−1)/2)`，偶数 `n` 为 `2^(n−1)−2^(n/2)`；严格几乎最优用严格大于。多输出 bent 的上界是偶数 `n` 下的 `2^(n−1)−2^(n/2−1)`。
3. **差分**：PN 要求 `δ_F=2^(n−m)`；`n=m` 且 `δ_F=2` 时为 APN。`δ(F)` 是最大绝对偏差，`sd(F)` 是对 `2^m(2^n−1)` 个非零差分单元的均方根；二者与 `δ_F` 分开显示。
4. **AB**：仅当 `n=m≥3`、`n` 为奇数、APN，且 `N_F=2^(n−1)−2^((n−1)/2)` 时标记。`x³` 在小奇数维可作回归向量。

## 8. 计算预算与验证

- 基础分量遍历约为 `O(2^(n+m))`；对全部非零输出掩码求 Walsh、生成 LAT 列约为 `O(n·2^(n+m))`。完整差分扫描约为 `O(2^(2n))` 时间；逐行 DDT 只需 `O(2^m)` 额外内存，逐列 LAT 只需 `O(2^n)` 额外内存。任一完整表均有 `2^(n+m)` 个单元，还需计入行容器、CSV 与缓存开销。精确 AI 另设更低上限。先按实际预算定默认 `n,m` 阈值，再在 CLI/API 中公开，超限返回 `OutOfRange`。
- 已知向量：书页 167 的例 3.2 是平衡 `(3,3)` 置换，可校验输出直方图及全部非零分量；另用 `(n,m)=(2,2)` 的输出序列 `00,00,11,11` 验证“坐标均平衡但整体不平衡”。恒等置换应满足 `DDT[a][b]=2^n` 当且仅当 `a=b`，以及 `LAT[a][b]=2^n` 当且仅当 `a=b`；它的差分最大概率、最佳线性近似概率均为 1，非线性度为 0。PRESENT 4×4 S 盒可用于核验最大 DDT 计数 4、差分最大概率 `1/4`、完整值 LAT 最大绝对值 8、最佳线性近似概率 `3/4`；小维 `x³` 可验证 APN/AB 条件；常量函数检验不平衡与退化分量。
- 公式自检：`Σ_β count[β]=2^n`；分量 `W(0)=2^n−2wt(f_c)`；每列 LAT 满足 Parseval；每个 `a` 的 `Σ_b DDT[a][b]=2^n`；`δ_F` 不小于非零差分行平均值；平衡等价于全体非零分量 `LAT[0][b≠0]=0`。用直接枚举公式与 FWHT 对照小维 LAT 每个单元，防止行列转置、半值约定或符号错误。
- 按需调用测试：只请求 DDT 时 Walsh/FWHT 计数为 0；只请求 LAT 时 DDT 行生成计数为 0；只请求最大线性偏差时不分配完整 LAT；同时请求非线性度与 LAT 时同一输出掩码的 Walsh 仅计算一次。全表超预算时校验未留下半成品 CSV。
- 等价变换测试应在可计算的小维函数上重新计算非线性度与差分均匀度，核验书页 171 的不变量陈述；次数在仿射退化情形下以实际重算值为准。

## 参考依据

- 张卫国《密码函数》第 3 章 3.1 节：多输出函数分量、Walsh、非线性度、差分分布与差分均匀度。
- Léo Perrin, [Tutorial on S-box Analysis](https://who.rocq.inria.fr/Leo.Perrin/teaching/tutorial-sbox.html)：DDT、LAT 的表格定义及带符号 Walsh 值与半值显示约定；本大纲明确采用完整 Walsh 值。
- Mitsuru Matsui, [Linear Cryptanalysis Method for DES Cipher](https://luca-giuzzi.unibs.it/corsi/Support/papers-cryptography/Matsui.pdf)：线性近似及偏差的原始攻击背景。
