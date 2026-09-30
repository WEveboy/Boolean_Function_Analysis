# 布尔函数安全性分析

Windows 桌面软件：侧边栏可切换「单输出布尔函数」和「多输出布尔函数」。React 界面负责导入、勾选和展示；Electron 提供本地文件操作；两套 C++ 引擎分别执行对应的函数转换及指标计算。分析在本机完成，结果写入用户选择的目录。

## 直接使用

双击 `BooleanFunctionLab-2.1.0-portable.exe`。程序为 Windows x64 便携版，无需单独安装 Node.js、React 或 C++ 编译器。先在侧边栏选择分析类型。单输出页面内仍可选择「单函数分析」或「双函数互相关」；多输出页面可分析向量函数与 S 盒，并按需导出 DDT / LAT 完整表。两个页面均可导入对应格式的真值表或 ANF TXT。拖入 TXT，也可点击「浏览」，勾选指标后按「开始分析」。

单输出结果默认保存到输入文件旁的 `bf_output`，多输出结果默认保存到 `vbf_output`；两个页面都可以另选文件夹。可直接导入的单输出与多输出 TXT 文件及对应设置见 [范例文件夹](examples/README.md)。图文简明手册可下载 [单输出使用说明 PDF](output/pdf/单输出布尔函数使用说明-v2.1.0.pdf) 和 [多输出使用说明 PDF](output/pdf/多输出布尔函数使用说明-v2.1.0.pdf)。多输出真值表、坐标 ANF 和指标说明见 [多输出使用说明](multi-output/README.md)。

单输出分析每次生成以下两个文件，顺序固定：

- `Truth_table.txt`：输入函数的二进制真值表；
- `ANF.txt`：输入函数的规范化代数正规型。

指标结果只在界面展示，不再写入 JSON 文件；较长的次数分布可在结果卡片中翻页查看。双文件互相关模式下，两份 TXT 对应函数 `f`，互相关指标显示在界面。EA 变换后已勾选的指标在界面另行显示，两份 TXT 仍对应原输入函数。目标目录已有任何内容时，软件新建 `run-2`、`run-3` 等子目录保存这一轮的两个文件，不覆盖旧结果。自相关结果包含零差分的绝对值极值，并额外报告非零差分的极值。

## TXT 输入约定

真值表按输入 `00…0` 到 `11…1` 的顺序排列，`x1` 是最高位；去掉文件中的普通空格、制表符、换行、不间断空格（U+00A0）、窄不间断空格（U+202F）和零宽空格等不可见字符后，长度须为 `2^n`。支持 `0b` 二进制前缀、`0x` 十六进制前缀和无前缀输入。无前缀且只含 `0`、`1` 时默认按二进制解释；若本意是十六进制，应加 `0x` 或在界面指定。十六进制每个字符展开为四个二进制位，保留前导零。

ANF 例子：`n=3; x1*x3 + x2 + 1`，也支持用户提供的 `f(x4,x3,x2,x1)=x4x3+x3x2+x2x1+x4x1+x3` 格式。函数头须完整列出 `x1` 至 `xn`，顺序不限。`+` 或 `⊕` 表示异或，`*` 可省略；仅支持变量、乘积、常数和异或，不支持表达式括号。空格、换行及复制公式常见的零宽空格会自动清除。常数函数必须写 `n=<变量数>;` 或在界面填入变量数。TXT 使用 UTF-8，可带 BOM。双文件互相关只接受进制相同、真值表长度相同的两份文件。

## 功能与算法

指标包括平衡性、代数次数、非线性度、Walsh 谱次数分布、相关免疫阶、弹性阶、Bent 判定、Plateaued 分类、代数免疫阶 AI、快速代数免疫阶 FAI、严格雪崩准则 SAC、扩散准则 PC(k)、全局雪崩特征 GAC、自相关次数分布、互相关次数分布和 EA 变换。互相关定义为 `C_fg(a)=Σ_x (-1)^(f(x)⊕g(x⊕a))`。

真值表与 ANF 互转使用快速 Möbius 变换；Walsh 谱与完整自相关、互相关使用快速 Walsh–Hadamard 变换。重复请求谱数据时复用同一次计算。SAC 和低阶 PC(k) 可只计算所需差分。AI 使用 GF(2) 方程组求精确值；FAI 按项目大纲的定义，对 `deg(g)<n/2` 的非零 `g` 穷举并最小化 `deg(g)+deg(fg)`，其中要求 `fg≠0`。

常规输入最多 20 元；精确 AI 最多 12 元，精确 FAI 最多 6 元。超过范围时对应结果标为“超出范围”，不会伪装为零。EA 矩阵按行输入，以英文逗号隔开，每行正好 `n` 位；行向量作用于输入变量，必须在 GF(2) 上可逆。

## 项目结构

```text
app/
  electron/main.cjs       Electron 窗口、文件对话框和 C++ 进程调用
  electron/preload.cjs    最小化的桌面 API 桥接
  src/main.jsx            共用侧边栏与单输出界面
  src/MultiOutput.jsx     多输出界面
  src/styles.css          界面样式
  src/multi-output.css    多输出界面样式
  index.html              前端入口
  vite.config.js          前端构建配置
  package.json            依赖和 Windows 便携版打包配置
  pnpm-lock.yaml          锁定依赖
  build/icon.ico          Windows 应用图标
cpp/
  bf_core.cpp             独立的解析、转换和指标函数及命令行入口
  build.ps1               MSVC 构建脚本
multi-output/cpp/
  vbf_core.cpp            多输出与 S 盒计算引擎
  build.ps1               多输出引擎构建脚本
tests/test_core.py        独立数学参考实现与端到端检查
tests/fixtures/anf_user_style.txt  用户提供的 ANF 格式回归样例
examples/                 两类 TXT 可导入范例与设置说明
output/pdf/               单输出与多输出使用说明 PDF
scripts/create_icon.py    图标生成脚本（需要 Pillow）
scripts/create_user_guides.py  两份 PDF 使用说明生成脚本（需要 ReportLab）
scripts/make_release.py  源码包与 SHA-256 清单生成脚本
C++功能实现文件大纲.md       算法与接口设计说明
单输出布尔函数安全性指标.xmind  安全性指标思维导图
```

## 从源代码构建

需要 Windows x64、Visual Studio 2022 C++ 工具、Node.js 和 pnpm。先运行：

```powershell
./cpp/build.ps1
./multi-output/cpp/build.ps1
cd app
pnpm install
pnpm approve-builds electron esbuild electron-winstaller
pnpm run build:web
pnpm exec electron-builder --win portable --x64
```

生成的便携版在根目录 `dist/`。开发时运行 `pnpm dev`。后端测试在项目根目录执行：

```powershell
python -m unittest discover -s tests -v
```

还可运行多输出引擎测试：`python -m unittest discover -s multi-output/tests -v`。测试需要先编译两套 C++ 引擎。C++ 后端也可独立调用，例如：

```powershell
./cpp/build/bf-core.exe --input example.txt --kind auto --radix auto --metrics balance,degree,autocorr_dist --out results
```

`--input2` 添加第二份真值表并配合 `--metrics crosscorr_dist`；`--pc-k` 设置扩散阶数；`--n` 指定 ANF 变量数。程序在标准输出返回 JSON，详细错误同样返回 JSON。

## 说明

源代码压缩包包含完整项目文件、测试、大纲和 XMind 导图，不包含书籍 PDF、依赖缓存、构建中间文件或已编译程序。便携版未做代码签名；首次运行可能出现 Windows 发布者提示。
