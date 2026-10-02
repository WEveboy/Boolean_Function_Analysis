# GitHub 更新与发布流程

本流程适用于 `WEveboy/Boolean_Function_Analysis`。日常源码更新进入 `main`；可下载的 Windows EXE、对应源码 ZIP 和校验文件放在带版本号的 GitHub Release。开始前先核对仓库当前内容与本地改动，避免覆盖别人或其他设备上的更新。

## 1. 确定本次上传范围

1. 对照任务列出需要更新的源码、测试、范例、文档及版本号；检查远端 `main` 的最新提交。
2. 保留项目需要的文件：`app/` 前端与 Electron 源码、`cpp/`、`multi-output/`、`tests/`、`examples/`、`docs/`、`scripts/`、`.github/workflows/`、根目录说明及两份用户手册 PDF。
3. 不提交构建和运行产物：`app/node_modules/`、`app/dist/`、`dist/`、`cpp/build/`、缓存、临时测试文件、分析输出及重复的独立版程序。以 `.gitignore` 为基础，再人工检查实际上传清单。
4. 范例 TXT 放入 `examples/single-output/` 或 `examples/multi-output/`，在 `examples/README.md` 写清文件格式、界面输入选项和预期用途。文档放入 `docs/`，需要时更新根目录 `README.md` 的入口。

## 2. 上传前验证

在仓库根目录编译并运行两套引擎的测试：

```powershell
./cpp/build.ps1
./multi-output/cpp/build.ps1
python -m unittest discover -s tests -v
python -m unittest discover -s multi-output/tests -v
```

前端改动还要在 `app/` 中执行：

```powershell
pnpm install --frozen-lockfile
pnpm run build:web
```

输入解析或界面导入改动，至少用 `examples/single-output/quadratic_4var_anf.txt` 与 `examples/multi-output/PRESENT_4x4_ANF.txt` 做对应模式的实际导入验证；相关真值表范例也应能正常分析。更新文档链接后，检查链接目标确实存在。

## 3. 更新 GitHub `main`

1. 再次读取远端 `main` 最新提交，与准备上传的文件逐项比较。只上传本次需要的文件；保留远端已有但本地没有的文件。
2. 使用已连接且有写入权限的 GitHub 账户或连接器，将这组改动作为内容明确的提交写入 `main`。如果本地 Git 登录不是仓库所有者 `WEveboy`，先核对实际可用的写入连接，不要假定登录已切换。
3. 上传后核对提交 SHA、文件清单与关键页面，并等待 [Windows CI](https://github.com/WEveboy/Boolean_Function_Analysis/actions/workflows/ci.yml) 完成。若失败，查看失败步骤并修复后重新验证。
4. 在交付说明中给出仓库提交或变更链接、测试和 CI 结果，以及尚未完成的事项。

## 4. 发布新的 Windows EXE 时

只有准备发布新版本时才执行本节。`main` 的后续更新不会自动改变已发布的 EXE 或源码 ZIP。

1. 确定版本号，更新 `app/package.json` 及需要同步的说明与手册；完成第 2 节的验证。
2. 先编译两套 C++ 引擎，再在 `app/` 构建 Windows x64 便携版：

   ```powershell
   pnpm exec electron-builder --win portable --x64
   ```

3. 核对 `dist/BooleanFunctionLab-<版本号>-portable.exe` 存在。若本次增减了应进入源码包的文件，先更新 `scripts/make_release.py` 的 `FILES` 清单，再运行：

   ```powershell
   python scripts/make_release.py
   ```

4. 验证源码 ZIP 中的文件清单、ZIP 完整性和 `SHA256SUMS.txt` 的哈希；用最终 EXE 实际打开单输出与多输出 ANF 范例，并确认两个模块均可分析。
5. 在 GitHub 创建与版本一致的 tag 和 Release，上传便携版 EXE、源码 ZIP、`SHA256SUMS.txt`，写明版本变化和下载说明。确认资产能访问、校验值匹配，并更新 README 的下载链接。

目前仓库未添加许可证；除非用户改变这一选择，后续上传不新增 `LICENSE`。
