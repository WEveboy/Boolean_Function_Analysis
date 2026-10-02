# 多输出布尔函数安全性分析

多输出功能现已集成到项目根目录 `app/` 的统一 Windows x64 桌面程序中，通过左侧「多输出布尔函数」进入。此目录保留多输出 C++ 引擎和测试；可导入范例统一放在根目录 `examples/multi-output/`。React 提供本地交互界面，Electron 负责文件选择和运行 C++ 引擎。分析输入不会上传。用户可以只勾选需要的指标；S 盒差分分布表 DDT、线性近似表 LAT 分别勾选导出。

## 直接使用

双击根目录 `dist/BooleanFunctionLab-2.1.0-portable.exe`，在侧边栏选择「多输出布尔函数」。在界面选择 TXT 文件、输入位数 `n`、输出位数 `m` 和输入类型；真值表还需选择进制。如果真值表按坐标函数逐行排列，勾选「转置真值表」；ANF 输入无需转置。勾选指标及可选的 DDT/LAT 完整表，点击「开始分析」。结果显示在界面；文件默认保存到输入文件旁的 `vbf_output`，也可另选文件夹。目标目录非空时自动新建 `run-2`、`run-3` 等子目录，不覆盖已有文件。

输入是一张按 `x=0,1,…,2^n−1` 排列的向量真值表，含 **恰好 `2^n` 个输出词**。`x1` 和 `f1` 为最高位。十六进制时每个词宽 `⌈m/4⌉` 位，二进制时每个词宽 `m` 位；词可由空格、换行、逗号或分号分隔。也可以无分隔符连续书写定宽词。输入允许 UTF-8 BOM，不支持在 TXT 中夹杂注释。

例如 4×4 PRESENT S 盒，`n=4`、`m=4`、十六进制：

```text
C 5 6 B 9 0 A D 3 E F 8 4 7 1 2
```

根目录的 `examples/multi-output/PRESENT_4x4_truth_hex.txt` 和 `examples/multi-output/PRESENT_4x4_truth_binary.txt` 是可直接导入的同一函数真值表；分别选择十六进制或二进制。`examples/multi-output/PRESENT_4x4_ANF.txt` 是同一函数的四个坐标 ANF，可直接导入并分析。

坐标 ANF 首行须为 `n=4; m=4`（数字按实际维数填写）；随后分别写 `f1 = ...` 至 `fm = ...`，每个坐标函数恰好出现一次。`+` 或 `⊕` 表示异或，`*` 可省略，支持常数 `0`、`1`。界面中的 `n`、`m` 必须与首行一致，输入类型可选「自动识别」或「代数正规型 ANF」。ANF 输入的进制及转置选项不参与解析。解析后与真值表走相同的指标、DDT/LAT 和导出流程。

若文件是**转置真值表**，勾选「转置真值表」后，每个非空行表示一个坐标函数，依次为 `f1` 至 `fm`；每行按 `x=0,1,…,2^n−1` 排列，共 `2^n` 位。二进制模式下每行写满 `2^n` 个 `0/1`，可用空格、逗号、分号或竖线分隔。十六进制模式下每行将这一整行位串从左到右每四位压成一位十六进制，写满 `⌈2^n/4⌉` 位，必须保留行首 0；当 `n=1` 时，高两位填 0。空白行忽略；不能在一行内夹杂注释。转置只在导入阶段进行，输出的 `Truth_table.txt` 仍是标准逐词格式。

同一个 4×4 PRESENT S 盒的转置十六进制输入如下，设置 `n=4`、`m=4`、十六进制并勾选「转置真值表」：

```text
9B70
E16C
32E5
59A6
```

根目录的 `examples/multi-output/PRESENT_4x4_transposed_hex.txt` 和 `examples/multi-output/PRESENT_4x4_transposed_binary.txt` 可直接导入；它们与上述逐词真值表应生成完全相同的 ANF、DDT 和 LAT。

8×8 S 盒的每个十六进制输出词须为两位，如 `00`、`A7`。输出位数不是四的倍数时，可用 `m` 位二进制词，也可用足够宽的十六进制词，超出 `m` 位的高位必须为 0。

## 输出文件

- `Truth_table.txt`：规范化后的 `n,m` 与逐行 `m` 位输出词。
- `ANF.txt`：每个坐标函数 `f1,…,fm` 的代数正规型。
- `DDT.csv`：仅勾选「差分分布表」时生成。行是输入差分 `a`，列是输出差分 `b`，单元格是计数 `#{x:F(x⊕a)⊕F(x)=b}`，包含 `a=0` 核验行。
- `LAT.csv`：仅勾选「线性近似表」时生成。行是输入掩码 `a`，列是输出掩码 `b`，单元格是**完整的带符号 Walsh 值** `Σ_x(-1)^(a·x⊕b·F(x))`，并非有些工具显示的半值；包含零掩码行列。

指标值只在界面展示，不另写结果 JSON。完整表有 `2^(n+m)` 个单元，单表上限为 131072 个单元；超过时可以只算摘要指标。源代码中的 C++ 引擎返回 JSON 供桌面界面消费。

## 指标与算法

基础指标有平衡性、最大/最小代数次数、代数免疫阶 AI、向量非线性度、弹性阶、多输出 Bent 和几乎最优分类。差分指标有差分均匀度、最大差分概率、最大偏差、标准差、PN/APN 分类。线性指标有最大相关幅度、最大绝对线性偏差、最佳线性近似概率。AB 分类需要差分均匀度与非线性度，只有请求时才联合计算。

分量 `f_b(x)=b·F(x)` 覆盖全部非零输出掩码 `b`。ANF 使用快速 Möbius 变换，LAT 列和非线性度使用快速 Walsh–Hadamard 变换，DDT 逐行扫描。仅请求差分指标不运行 Walsh；仅请求线性指标不扫描 DDT。批量请求会复用同一分量谱或 DDT 行。

精确引擎支持 `1≤n≤12`、`1≤m≤min(n,8)`；AI 的精确求解另外限制 `n≤8`、`m≤6`，超限时该指标明确显示「超出范围」。单次任务在桌面进程中的超时为 5 分钟。大维度的全分量或差分扫描可能较慢，建议先选摘要指标并从小维度开始。

## 从源代码构建

需要 Windows x64、Visual Studio 2022 C++ 工具集、Node.js 和 pnpm。在项目根目录运行：

```powershell
./cpp/build.ps1
./multi-output/cpp/build.ps1
cd app
pnpm install
pnpm run build:web
pnpm exec electron-builder --win portable --x64
```

构建后的便携版位于 `dist/`。若 pnpm 提示忽略 Electron 安装脚本，请先允许该脚本并确认 `app/node_modules/electron/dist/electron.exe` 存在。C++ 后端可独立调用：

```powershell
./multi-output/cpp/build/vbf-core.exe --input examples/multi-output/PRESENT_4x4_truth_hex.txt --n 4 --m 4 --radix hex --metrics balance,nonlinearity,diff_uniformity,linearity --ddt --lat --out results
```

转置输入在命令行中加 `--transpose`，例如：

```powershell
./multi-output/cpp/build/vbf-core.exe --input examples/multi-output/PRESENT_4x4_transposed_hex.txt --n 4 --m 4 --radix hex --transpose --metrics diff_uniformity,linearity --ddt --lat --out results
```

后端自检：

```powershell
python -m unittest discover -s multi-output/tests -v
```

## 文件结构

```text
app/                             统一桌面界面与打包配置
multi-output/cpp/vbf_core.cpp    多输出 C++ 引擎与命令行入口
multi-output/cpp/build.ps1       MSVC 编译脚本
multi-output/tests/             独立参考公式与回归用例
examples/multi-output/          PRESENT 4×4 的真值表与 ANF 范例
scripts/make_release.py         完整源码包与 SHA-256 清单
```

进一步的设计资料见 [多输出 XMind 导图](../docs/design/多输出布尔函数安全性指标.xmind) 和 [C++ 功能实现大纲](../docs/design/多输出布尔函数_C++功能实现文件大纲.md)。EXE 未做代码签名，首次运行可能显示 Windows 发布者提示。
