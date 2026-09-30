# 单输出布尔函数安全性指标：C++ 功能实现文件大纲

依据张卫国《密码函数》第 2 章 2.1 节（书页 51–63）及同目录的 XMind 导图设计。目标是从拖入的 TXT 文件识别真值表或代数正规型（ANF），统一生成二进制真值表和 ANF 文本；各安全性指标及频谱、自相关、互相关统计通过独立接口按需计算。

## 1. 输入、识别与标准化输出

- `f : F₂ⁿ → F₂`；真值表长度必须为 `2^n`，按输入 `00…0` 到 `11…1` 的二进制值升序排列，`x₁` 是最高位。
- **单文件流程 A：真值表 TXT**。拖入后读取 UTF-8（允许 BOM），删除空格、制表符、回车、换行等空白字符；识别 `0b` 前缀的二进制或 `0x` 前缀的十六进制，未带前缀时只含 `0/1` 的默认识别为二进制、含 `2–9` 或 `A–F/a–f` 的识别为十六进制。十六进制每字展开 4 位并保留前导零。展开后的位数必须为 `2^n`；十六进制形式因此要求 `n≥2`。其他字符报出位置与原因。
- **进制歧义**。仅由 `0/1` 构成的 TXT 也可能是十六进制，无法仅凭内容可靠判断。软件默认二进制，同时显示识别结果；要按十六进制解释，可在文件中加 `0x` 前缀或在拖入界面显式选择。不得静默猜测另一种长度。
- **单文件流程 B：ANF TXT**。拖入后删除空白字符及复制公式常见的不间断空格（U+00A0）、窄不间断空格（U+202F）、零宽空格等不可见字符；识别 `x1*x3+x2+1`、`x1x3⊕x2⊕1`，以及 `f(x4,x3,x2,x1)=x4x3+x3x2+x2x1+x4x1+x3` 等 ANF。函数头完整列出 `x1` 至 `xn`，可自动推断元数。`+` 与 `⊕` 表示 GF(2) 加法，`*` 或变量紧邻表示乘法；不接受表达式括号和一般代数表达式。重复单项式按 XOR 抵消，同一单项式中的重复变量按 `x_i²=x_i` 化简。各项必须由 `+`/`⊕` 明确分隔，换行本身不是加号。
- ANF 的变量编号限定为 `x1` 至 `xn`，内部子集 bitmask 中 `x_i` 对应位 `n−i`，与真值表中 `x₁` 为最高位的约定一致；`x0`、下标越界和空乘积写法均给出语法错误。
- ANF 可用文件开头 `n=4;` 声明元数，也可在拖入界面指定。未指定时由最大变量下标推断 `n`，界面明确展示该推断；如果函数缺少最高编号变量，必须提供 `n` 才能得到预期长度。常数 ANF `0`/`1` 必须提供 `n`。
- 单独的 `0` 或 `1` 同时可能是真值表片段和常数 ANF；按 ANF 处理时需在界面选择“代数正规型”或写入 `n=<值>;`。自动识别只对无歧义内容生效，界面显示最终识别的输入类型。
- 自动分类先检查 `0b`/`0x` 前缀，再检查 `n=<值>;`、`x<数字>`、`+`、`⊕` 等 ANF 语法，最后按纯数字真值表处理；显式界面选择优先，但若内容与所选类型冲突则报错。这样 `0x` 不会被误判成变量 `x`。
- **双文件流程 C：互相关**。仅接收两份真值表 TXT；分别执行流程 A 的空白清理和进制识别。两份文件必须采用同一进制，且清理、解码后的真值表长度相等；未满足条件即拒绝计算，并报告各自进制与长度。输入顺序固定为 `f`、`g`，输出标注两份源文件。
- 上述流程成功后只导出 `Truth_table.txt` 和 `ANF.txt` 两个文件，顺序固定，均对应主输入函数 `f`。前者仅含按规定顺序排列的 `2^n` 个二进制位及结尾换行；后者包含 `n=<值>;` 和规范化、稳定排序的 ANF 表达式。输出目录由用户选择或默认为源文件旁的 `bf_output`；若目录已有内容则创建 `run-2` 等新子目录，不覆盖旧文件。互相关指标、EA 变换后指标在界面显示，不额外生成文件。
- 双文件导入应先完成双方校验，再写出任何结果；写 TXT 采用临时文件写完后同目录原子改名，避免一份文件失败时留下半成品。
- 转换是导出要求：真值表输入需要一次快速 Möbius 变换生成 ANF；ANF 输入先构造系数向量，再经同一变换生成真值表。除这项必需转换外，不预先计算 Walsh、自相关或其他指标。
- 常规变换上限建议为 `n ≤ 20`，由内存与运行时间预算再限制；代数免疫的精确求解默认 `n ≤ 12`，快速代数免疫的穷举默认 `n ≤ 6`。超过对应上限时返回“未计算/超出精确算法范围”，不能把空结果当作 0。
- 零函数的代数次数采用内部哨值 `-1`，报告中写“未定义（零多项式）”；常数 1 的次数为 0。

## 2. 文件与职责：按计算依赖拆分

```text
include/bf/boolean_function.hpp    真值表类型、输入约定与校验
include/bf/txt_import.hpp          拖入文件读取、空白清理、进制/ANF识别
include/bf/txt_export.hpp          二进制真值表和规范 ANF 的 TXT 输出
include/bf/balance.hpp             汉明重量、平衡性
include/bf/anf.hpp                 ANF 解析、快速 Möbius 变换、代数次数
include/bf/walsh.hpp               FWHT、非线性度、谱分类、谱值次数分布
include/bf/correlation.hpp         相关免疫阶、弹性阶
include/bf/autocorrelation.hpp     指定差分、自相关谱及数值分布
include/bf/cross_correlation.hpp   两函数互相关谱及数值分布
include/bf/avalanche.hpp           PC(k)、SAC、GAC
include/bf/algebraic.hpp           AI、FAI 的独立精确算法
include/bf/ea_transform.hpp        给定 EA 参数的真值表变换
include/bf/analysis_session.hpp    可选的按需缓存与请求分发
include/bf/report.hpp              各指标结果及其计算状态
ui/drop_controller.hpp              单/双文件拖入、识别结果预览与歧义选择
src/*.cpp                           每个头文件对应一个实现文件
ui/drop_controller.cpp              把拖入路径转给 txt_import，不重复解析
src/main.cpp                        拖入文件路径、双文件与 --metric/--all 分发
tests/import_export.cpp              两类输入与双文件检查、往返测试
tests/known_vectors.cpp             单指标、三类分布及跨公式验证
```

每个指标函数只接收其真正需要的输入，能够单独调用；`analysis_session` 仅供批量请求复用已算出的 ANF、Walsh、自相关或两份 Walsh 谱，不承担指标公式本身。拖入界面把文件路径传给同一导入接口；CLI 也可直接接收文件路径。`main.cpp` 除标准化导出所必需的转换外，不预先计算其他指标。

## 3. 核心数据结构与接口草案

```cpp
struct BooleanFunction {
    unsigned n;
    std::vector<std::uint64_t> packed_truth; // 第 x 位为 f(x)，x₁ 为最高位
    static BooleanFunction from_bits(std::string_view bits);
    static BooleanFunction from_hex(unsigned n, std::string_view hex);
    std::size_t size() const noexcept; // 2^n
    bool value(std::size_t x) const noexcept;
};

enum class InputKind { TruthTable, Anf };
enum class Radix { Binary, Hexadecimal, NotApplicable };
struct ImportedFunction {
    BooleanFunction function;
    std::vector<std::uint8_t> anf; // 导出所需，真值表输入时由快速 Möbius 变换得到
    InputKind kind;
    Radix source_radix;
    std::filesystem::path source_path;
};
ImportedFunction import_txt(const std::filesystem::path& path,
                            std::optional<InputKind> kind = std::nullopt,
                            std::optional<Radix> radix = std::nullopt,
                            std::optional<unsigned> n = std::nullopt);
std::pair<ImportedFunction, ImportedFunction> import_truth_pair_txt(
    const std::filesystem::path& first, const std::filesystem::path& second,
    std::optional<Radix> radix = std::nullopt);
void export_normalized_txt(const ImportedFunction& input,
                           const std::filesystem::path& output_directory);

enum class ComputeStatus { Exact, OutOfRange, NotApplicable };
template<class T> struct ExactResult { ComputeStatus status; std::optional<T> value; };
struct Limits { unsigned max_n; };
struct GacResult { std::uint32_t delta; std::uint64_t sigma; };
struct ValueDistribution {
    std::vector<std::pair<std::int32_t, std::uint64_t>> counts; // 按有符号值升序
    std::uint32_t min_abs, max_abs;
    std::uint32_t first_min_index, first_max_index; // 输入谱的 alpha 或差分 a
};
struct AutocorrelationDistribution {
    ValueDistribution all_shifts;      // 包含 a=0
    std::optional<ValueDistribution> nonzero_shifts; // n>=1 时存在
};
enum class MetricId { Balance, Degree, Nonlinearity, Resiliency,
                      AlgebraicImmunity, FastAlgebraicImmunity, Sac, Pc, Gac,
                      WalshDistribution, AutocorrelationDistribution };
// MetricValue 是上述指标的类型化结果变体，连同 ComputeStatus 返回。

std::uint32_t hamming_weight(const BooleanFunction& f);
bool is_balanced(const BooleanFunction& f);

std::vector<std::uint8_t> anf_coefficients_fast(const BooleanFunction& f);
BooleanFunction truth_from_anf_fast(unsigned n,
    std::span<const std::uint8_t> anf); // Möbius 变换在 GF(2) 上自逆
int algebraic_degree(std::span<const std::uint8_t> anf);

std::vector<std::int32_t> walsh_spectrum(const BooleanFunction& f);
ValueDistribution walsh_frequency_distribution(
    std::span<const std::int32_t> walsh);
std::uint32_t nonlinearity(std::span<const std::int32_t> walsh);
unsigned correlation_immunity_order(std::span<const std::int32_t> walsh);
std::optional<unsigned> resiliency_order(std::span<const std::int32_t> walsh);

std::int32_t autocorrelation_at(const BooleanFunction& f, std::uint32_t mask);
std::vector<std::int32_t> autocorrelation_spectrum(
    std::span<const std::int32_t> walsh);
AutocorrelationDistribution autocorrelation_value_distribution(
    std::span<const std::int32_t> autocorr);
std::vector<std::int32_t> cross_correlation_spectrum(
    std::span<const std::int32_t> walsh_f,
    std::span<const std::int32_t> walsh_g);
ValueDistribution cross_correlation_value_distribution(
    std::span<const std::int32_t> crosscorr);
class AnalysisSession;
ValueDistribution analyze_cross_correlation(AnalysisSession& f,
    AnalysisSession& g); // 双函数按需入口
bool satisfies_sac(const BooleanFunction& f); // 仅检查 n 个单位差分
bool satisfies_pc(const BooleanFunction& f, unsigned k); // 仅检查所需差分
GacResult gac(std::span<const std::int32_t> autocorr);

ExactResult<unsigned> algebraic_immunity(const BooleanFunction& f, Limits limits);
ExactResult<unsigned> fast_algebraic_immunity(const BooleanFunction& f, Limits limits);

class AnalysisSession { // 可选门面：首次访问时计算并缓存
public:
    explicit AnalysisSession(const ImportedFunction& input);
    const std::vector<std::uint8_t>& anf();
    const std::vector<std::int32_t>& walsh();
    const std::vector<std::int32_t>& autocorr(); // 首次请求时使用 walsh()
    MetricValue evaluate(MetricId id);          // 只调度目标指标
private:
    const ImportedFunction& input_;
    std::optional<std::vector<std::int32_t>> walsh_;
    std::optional<std::vector<std::int32_t>> autocorr_;
};
```

`MetricValue` 是带指标名、数值和状态的类型化结果；`ExactResult` 能表示已精确计算、超限、无定义。`AnalysisSession::anf()` 直接返回导入阶段已有的系数；`walsh()`、`autocorr()` 才按首次请求计算。互相关由两个 `AnalysisSession` 分别取得 Walsh 谱后计算，不触发各自的自相关。`autocorr` 可按 `C = FWHT(W²)/2^n` 计算；互相关可按 `C_fg = FWHT(W_f·W_g)/2^n` 计算。乘积与 FWHT 使用 64 位有符号中间量，检查整除和最终范围。`n ≤ 20` 时 `gac_sigma` 的最坏值 `2^(3n)` 仍可放入 `uint64_t`；若直接检查 `ΣW⁴=2ⁿσ`，使用 128 位或多精度整数。

## 4. 新增的三个统计接口

统一约定：`N=2^n`，`W_f(u)=Σ_x(-1)^{f(x)⊕u·x}`，频次表按**带符号的值**计数，例如 `−4:1, 0:4, 4:3`；另报告绝对值极值。所有频次之和必须等于 `N`，极值的下标是首次出现的位置。

| 接口 | 计算与返回 | 复杂度 |
|---|---|---|
| `walsh_frequency_distribution(W_f)` | 单次扫描 Walsh 谱，计数每个谱值出现次数；返回升序频次表及绝对值最大/最小值。 | 已有谱时 `O(N)`。 |
| `autocorrelation_value_distribution(C_f)` | 计数 `C_f(a)=Σ_x(-1)^{f(x)⊕f(x⊕a)}` 的每个值，返回全部差分上的绝对值最大/最小值及首次对应的 `a`；另给 `a≠0` 的极值，避免 `C_f(0)=N` 掩盖有效扩散表现。 | 已有自相关谱时 `O(N)`。 |
| `cross_correlation_value_distribution(C_fg)` | 计数 `C_fg(a)=Σ_x(-1)^{f(x)⊕g(x⊕a)}` 的每个值，返回全部 `a` 上的绝对值最大/最小值及首次对应的 `a`。仅由双真值表导入流程调用。 | 已有互相关谱时 `O(N)`。 |

获取完整谱时，自相关使用 `C_f=FWHT(W_f²)/N`；互相关使用 `C_fg=FWHT(W_f·W_g)/N`。均为 `O(nN)`，比逐个差分直接求和的 `O(N²)` 更适合完整分布统计。谱值乘积先放入 64 位数组并原地变换；频次统计共用内部计数器，但保留三个独立公共接口。若只请求一个或少量自相关点，仍调用直接差分函数，避免完整变换。

互相关是本次增加的双函数功能；原 PDF 的 2.1 节定义的是单函数自相关。互相关在本大纲中明确采用上式，便于代码和报告使用同一约定。

## 5. 原有指标函数与计算依据

| XMind 分支 | 建议接口 | 核心计算与输出 |
|---|---|---|
| 平衡性 | `is_balanced(f)` | 直接计数 `wt(f)=2^(n-1)`；不触发 Walsh 计算。需要交叉校验时才使用 `W(0)=2^n−2wt(f)`。 |
| 代数次数 | `algebraic_degree(ANF)` | 对非零 ANF 系数对应的子集求最大 `popcount`；常数和零函数单独处理。 |
| 非线性度 | `nonlinearity(W)` | `2^(n-1)−max(abs(W))/2`；同时可分类 bent、plateaued、半 bent、几乎最优及严格几乎最优。分类器须按书中奇偶 n 的阈值执行。 |
| 相关免疫/弹性 | `correlation_order(W)`、`resiliency_order(W)` | 检查低重量非零谱点是否全为 0；弹性还要求 `W(0)=0`。报告最大满足阶及每一阶判定。 |
| 代数免疫 AI | `algebraic_immunity(f, limit)` | 对 `f` 和 `f⊕1`，逐次数 `d` 构造候选 ANF 单项式，在各自支撑集上建立 GF(2) 齐次线性方程；首次出现非零核向量时返回 `d`。 |
| 快速代数免疫 FAI | `fast_algebraic_immunity(f, limit)` | 按原书文字操作化：精确小规模遍历非零且 `deg(g)<n/2` 的候选 `g`（包含常数 1），求 `h=f·g≠0`，最小化 `deg(g)+deg(h)`；把搜索范围与状态写入报告。 |
| 扩散准则/SAC | `satisfies_pc(f,k)`、`satisfies_sac(f)` | 只枚举请求阶数内的差分并调用 `autocorrelation_at(f,a)`；单独检查 SAC 无须完整自相关谱。若请求最大 PC 阶，可复用完整谱。 |
| GAC | `gac(C)` | `Δ=max_{a≠0}|C(a)|`，`σ=Σ_a C(a)^2`，平方和包含 `a=0`。 |
| EA 变换 | `apply_ea(f,A,a,beta,epsilon)` | 用户提供 GF(2) 可逆 `A` 后变换真值表；核对非线性度、GAC 的 `Δ/σ`，并重新计算次数、AI、FAI（仅在精确算法范围内）。指标相同不能反推 EA 等价。 |

原书书页定位：平衡性 53、57；次数 55；非线性度和谱 55–60；相关免疫与弹性 58–61；AI/FAI 61–62；自相关和 GAC 62–63；EA 变换 63。

**EA 适用性校注**：原书引理 2.20 把次数、AI、FAI 也列为一般 EA 不变量；代码不宜据此写恒等断言。非仿射函数的次数不受输出仿射项影响，但仿射函数的次数可能改变。AI 的反例是 3 元 `f=x₁x₂` 与 `f⊕x₃`：两者由加入输出线性项得到，AI 分别为 1 和 2。AI/FAI 对单纯的可逆输入仿射代换可作为不变量；加入输出仿射项后应重新计算。

## 6. 按需调用与共享规则

1. **导入并生成双 TXT**：单文件走真值表或 ANF 解析；双文件走两个真值表解析并校验同进制、同长度。每个成功导入的函数都完成真值表与 ANF 的相互转换，主输入函数只导出 `Truth_table.txt` 与 `ANF.txt`。这一步只做用户要求的必需转换。
2. **读取计算请求**：拖入界面或 CLI 用可重复的 `--metric balance|degree|nonlinearity|resiliency|ai|fai|sac|pc|gac|walsh-dist|autocorr-dist|crosscorr-dist` 选择指标；`--all` 才请求全部适用的单函数指标。`crosscorr-dist` 要求恰好两个真值表文件，其他指标可独立调用。CLI 提供 `--input`、`--input2`、`--kind truth|anf`、`--radix bin|hex`、`--n`、`--output-dir`。
3. **按依赖分发**：平衡性直接使用位打包真值表；次数使用导入时已有 ANF；非线性度、相关免疫、弹性、Walsh 次数分布才请求 Walsh；SAC/低阶 PC 只计算需要的差分；GAC、自相关分布才请求完整自相关；互相关分布只请求两份 Walsh 并生成一份互相关谱。AI/FAI 各自执行独立搜索。
4. **复用同次分析的结果**：同一函数的多个谱指标共享一次 FWHT；自相关复用其 Walsh；互相关复用双方各自的 Walsh，但不会隐式计算双方自相关。标准化导出所生成的 ANF 直接交给次数接口，不重复做 Möbius 变换。
5. **展示指标结果**：每项分别附数值、频次、绝对值极值与计算状态。指标结果经进程内 JSON 返回给界面，较长分布分页显示；不写第三个结果文件。`--exact-ai`、`--exact-fai` 只是对应搜索的限制参数，不隐式触发搜索。

调用示例：`--input f.txt` 仅生成二进制真值表与 ANF 两份 TXT；`--input f.txt --metric balance` 再做位计数；`--input f.txt --metric walsh-dist --metric autocorr-dist` 共享一次 Walsh；`--input f.txt --input2 g.txt --metric crosscorr-dist` 校验双文件后做互相关。

```cpp
auto input = import_txt("f.txt");
export_normalized_txt(input, "bf_output"); // 必需的两份 TXT
AnalysisSession session(input);
auto counts = walsh_frequency_distribution(session.walsh());
auto ac = autocorrelation_value_distribution(session.autocorr()); // 复用 Walsh
```

### 算法选择与性能边界

| 计算 | 首选实现 | 避免的重复/低效工作 |
|---|---|---|
| 真值表 → ANF、ANF → 真值表 | 系数数组原地执行快速子集 Möbius 变换，`O(nN)` 时间、`O(N)` 临时空间；GF(2) 上同一变换可用于逆转换。 | 不枚举每个输入的全部单项式；同次导入只转换一次。 |
| 代数次数 | 在导出时得到的 ANF 系数上扫描非零项，以 `std::popcount(mask)` 取最高次数，`O(N)`。 | 不重新解析 ANF 或再次做 Möbius。 |
| 汉明重量、平衡性 | 对位打包真值表使用 `std::popcount`，`O(N/64)` 个机器字。 | 不调用 Walsh。 |
| Walsh 及谱指标 | 先构造 `(-1)^f` 数组，原地 FWHT，`O(nN)`；同次分析缓存一份谱。非线性度取单次 `max|W|`；相关免疫阶由最小的非零谱点重量确定；按需统计频次。 | 不对每个频点直接求 `O(N²)` 的和，不重复 FWHT。 |
| 少量差分、SAC、低阶 PC | 直接遍历需要的差分；位打包 XOR 与 `popcount`，在实现可用时按块处理。 | 无须计算完整自相关谱。 |
| 完整自相关、GAC、分布 | `FWHT(W_f²)/N`，`O(nN)`；若 Walsh 已在缓存中，只做谱平方和第二次变换。 | 不逐个差分扫描真值表。 |
| 完整互相关、分布 | 双方 Walsh 逐点相乘后 FWHT、除以 `N`，`O(nN)`；双方谱分别缓存。 | 不对每个差分做 `O(N²)` 枚举。 |
| AI | 按次数递增构造 GF(2) 位打包矩阵并消元，一旦找到非零核向量即停止。 | 不穷举全部候选函数。 |
| FAI | 在明确的小规模精确范围内用位打包真值表、Gray 码更新候选、当前最优值剪枝；超过范围返回超限状态。 | 不启动不可控的全规模穷举。 |
| EA 变换 | 给定可逆矩阵后按 Gray 码遍历输入，增量更新 `xA⊕a`，用奇偶校验计算输出仿射项；输出真值表后仅重新计算用户请求的指标。 | 不为每个输入重新做完整矩阵乘法或默认重算全部指标。 |

“最优算法”以本大纲的任务、输入规模和内存限制为准：完整谱采用快速变换，少量差分采用直接法。实现时根据所需差分个数和 Walsh 是否已缓存比较代价，选择更省时的路径；基准测试后再确定具体切换阈值，不声称存在对所有规模都最快的单一算法。

## 7. 验证用例与不变量

- 真值表 TXT：`0 0\n0 1` 清理后为 `0001`；`0x1` 展开也为 `0001`。两者应导出相同的 `Truth_table.txt` 和含 `n=2;x1*x2` 的 `ANF.txt`（允许规范化格式中的固定换行）。仅含 `0/1` 的十六进制例子必须通过 `0x` 或界面显式指定。
- ANF TXT：`n=2; x1 * x2\n` 生成 `0001`；重复项 `x1+x1+x2` 规范化为 `x2`。`x1` 若未指定 `n`，应提示推断为 1 元函数；常数 ANF 未指定 `n` 应报错。
- 双文件：相同进制、相同位数才可计算互相关；二进制 `0001` 与十六进制 `0x1` 虽代表同一真值表仍应按用户要求拒绝混合进制。位数不同、非法字符、错误 `n` 均应给出可定位的错误。
- `n=2, f=x₁x₂`：真值表 `0001`，次数 2，非线性度 1，Walsh 谱各项绝对值均为 2；是 bent，但不平衡。
- 该函数的 Walsh 次数分布应为 `−2:1, 2:3`；自相关分布应为 `0:3, 4:1`。全部差分上 `min|C|=0, max|C|=4`；非零差分上两者均为 0。把 `f` 同时作为两个输入时，互相关谱应逐项等于自相关谱。
- `f=0000`、`g=0001`（均为二元二进制真值表）时，互相关的四个值都为 2，频次分布为 `2:4`，绝对值最大和最小均为 2。
- `n=2, f=x₁⊕x₂`：真值表 `0110`，平衡，次数 1，非线性度 0；不满足 SAC。
- `n=3, f=x₁⊕x₂⊕x₃`：真值表 `01101001`，平衡，次数 1，非线性度 0，相关免疫与弹性阶均为 2。
- 所有已计算函数验证 `W(0)=2^n−2wt(f)`、`ΣW²=2^(2n)`、`C(0)=2^n`、`W²=FWHT(C)`；可用多精度再核对 `ΣW⁴=2^nσ`。
- 用计数器或注入式变换函数验证按需行为：导入真值表并导出两份 TXT 时 Möbius 恰好运行 1 次、FWHT 为 0；只追加平衡性不增加变换次数；只请求 SAC 时不生成完整自相关谱；同时请求 Walsh 分布、自相关分布和互相关分布时每个输入的 Walsh 最多计算 1 次。
- 对随机小规模函数，将快速 Möbius、FWHT 自相关和 FWHT 互相关与直接枚举结果逐项比较；每个分布的频次和必须为 `2^n`，极值须等于直接扫描结果。用 `n` 增大时的基准测试验证完整谱路径呈 `O(n2^n)` 增长，并记录峰值内存。
- 对给定可逆 EA 变换，测试非线性度及 GAC 不变；加入上述 AI 反例测试，防止误把 AI 当成一般 EA 不变量。若 `AI/FAI` 因规模限制未计算，仅核对已得到的指标。

## 8. 交付顺序

先实现两类 TXT 导入、双文件约束及二进制真值表/ANF 双向导出；再实现快速 Möbius、Walsh、自相关和互相关及三个分布接口；随后加入按需缓存的 `AnalysisSession`、原有指标与拖入界面/CLI 调度，最后加入 AI、FAI 和 EA 模块。每个接口都可单独调用，批量分析仅复用必需的中间结果。
