# 布尔函数实验室 · Boolean Function Lab

[![Windows CI](https://github.com/WEveboy/Boolean_Function_Analysis/actions/workflows/ci.yml/badge.svg)](https://github.com/WEveboy/Boolean_Function_Analysis/actions/workflows/ci.yml)

面向布尔函数与 S 盒分析的 Windows x64 桌面工具。左侧栏切换单输出、多输出模块，导入真值表或代数正规型（ANF）的 TXT 文件，按需计算密码学指标。计算和文件读写均在本机完成。

**[下载 v2.1.0 Windows 便携版](https://github.com/WEveboy/Boolean_Function_Analysis/releases/tag/v2.1.0)** · [查看 TXT 范例](examples/README.md) · [使用说明](docs/USER_GUIDE.md) · [从源码构建](docs/DEVELOPMENT.md)

## 界面预览

![单输出分析界面，左侧可切换多输出模块](docs/images/single-output.png)

<details>
<summary>查看多输出分析界面</summary>

![多输出分析界面](docs/images/multi-output.png)

</details>

## 主要功能

| 模块 | 输入 | 分析与导出 |
| --- | --- | --- |
| 单输出 | 二进制／十六进制真值表、ANF；支持双函数互相关 | 平衡性、代数次数、非线性度、Walsh 谱、自相关、AI、FAI、SAC、PC、EA 变换等；导出规范化真值表和 ANF |
| 多输出 | 逐词或转置真值表、逐坐标 ANF | 向量函数及 S 盒的代数、差分和线性指标；可选导出 DDT、LAT、真值表和 ANF |

## 三步开始

1. 从 [Releases](https://github.com/WEveboy/Boolean_Function_Analysis/releases/tag/v2.1.0) 下载 `BooleanFunctionLab-2.1.0-portable.exe`，在 Windows x64 上直接运行。
2. 在侧边栏选择模块，导入对应 TXT 文件，设置输入类型、维数和所需指标。
3. 点击「开始分析」。结果在界面展示，规范化文件保存在输入文件旁，也可另选目录。

第一次尝试可直接使用以下文件：

- [单输出 ANF：4 元二次函数](examples/single-output/quadratic_4var_anf.txt)
- [单输出真值表：同一函数](examples/single-output/quadratic_4var_truth_binary.txt)
- [多输出 ANF：PRESENT 4×4 S 盒](examples/multi-output/PRESENT_4x4_ANF.txt)
- [多输出真值表：同一 S 盒](examples/multi-output/PRESENT_4x4_truth_hex.txt)

所有范例及对应界面设置见 [examples/README.md](examples/README.md)。单输出与多输出的 PDF 手册分别见 [单输出使用说明](output/pdf/单输出布尔函数使用说明-v2.1.0.pdf) 和 [多输出使用说明](output/pdf/多输出布尔函数使用说明-v2.1.0.pdf)。

## 文档与源码

- [使用说明](docs/USER_GUIDE.md)：输入格式、输出文件、范围与常见问题。
- [多输出指标说明](multi-output/README.md)：坐标函数、DDT／LAT 和算法约定。
- [开发与构建](docs/DEVELOPMENT.md)：目录结构、编译、测试和发布包。
- [GitHub 更新与发布流程](docs/UPLOAD_WORKFLOW.md)：文件筛选、验证、上传及 EXE 发布核对。
- [单输出 C++ 设计大纲](docs/design/单输出布尔函数_C++功能实现文件大纲.md) · [多输出 C++ 设计大纲](docs/design/多输出布尔函数_C++功能实现文件大纲.md) · [指标思维导图](docs/design/)。

`main` 分支保存持续更新的源码；[v2.1.0 Release](https://github.com/WEveboy/Boolean_Function_Analysis/releases/tag/v2.1.0) 中的 EXE 与源码 ZIP 是该版本发布时的快照。发布包附带 `SHA256SUMS.txt`，可用于核对下载文件。
