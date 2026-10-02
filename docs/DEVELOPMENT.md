# 开发与构建

项目由一个 Electron／React 桌面界面和两套独立 C++ 计算引擎组成。单输出引擎在 `cpp/`，多输出引擎在 `multi-output/cpp/`；对应的端到端测试分别在 `tests/` 和 `multi-output/tests/`。

## 环境

- Windows x64，Visual Studio 2022 C++ 工具集（包含 MSVC）。
- Node.js 22、pnpm 11、Python 3.12。Python 测试仅使用标准库。

## 编译与测试

在仓库根目录运行：

```powershell
./cpp/build.ps1
./multi-output/cpp/build.ps1
python -m unittest discover -s tests -v
python -m unittest discover -s multi-output/tests -v
```

测试需要先编译两套 C++ 引擎。GitHub Actions 中的 [Windows CI](../.github/workflows/ci.yml) 会执行相同步骤，并构建前端。

进入 `app/` 安装依赖并构建页面：

```powershell
cd app
pnpm install --frozen-lockfile
pnpm run build:web
```

本地开发可在 `app/` 运行 `pnpm dev`。生成 Windows 便携版前，确保两套 C++ 引擎均已编译，然后在 `app/` 运行 `pnpm exec electron-builder --win portable --x64`。生成的 EXE 位于仓库根目录 `dist/`。

如果 pnpm 提示忽略 Electron 或 esbuild 的安装脚本，检查 `app/pnpm-workspace.yaml` 中的 `allowBuilds` 配置并重新安装依赖；打包前确认 `app/node_modules/electron/dist/electron.exe` 存在。

## 引擎命令行示例

从仓库根目录运行单输出范例：

```powershell
./cpp/build/bf-core.exe --input examples/single-output/quadratic_4var_anf.txt --kind auto --radix auto --metrics balance,degree,nonlinearity
```

运行多输出 PRESENT 4×4 真值表范例：

```powershell
./multi-output/cpp/build/vbf-core.exe --input examples/multi-output/PRESENT_4x4_truth_hex.txt --n 4 --m 4 --radix hex --metrics balance,nonlinearity,diff_uniformity,linearity
```

用户可在界面中选择输出目录；命令行参数与完整格式见根目录及 `multi-output/` 中的源代码和说明。

## 发布文件

`scripts/make_release.py` 读取文件清单并生成源码 ZIP 与 `SHA256SUMS.txt`。它要求 `dist/` 中已经存在与 `app/package.json` 版本号一致的便携版 EXE。构建产物、依赖缓存及临时结果不会提交到 `main`；已发布版本的 EXE、源码 ZIP 和校验值作为 Release 资产保存。
