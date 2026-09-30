# TXT 导入范例

本文件夹中的 `.txt` 可直接导入项目根目录的统一桌面程序。打开程序后，先在左侧选择「单输出布尔函数」或「多输出布尔函数」，再按下表选择对应文件和设置。TXT 文件只放输入数据；**不要把本说明文字复制进 TXT**。

可打印的操作步骤：[单输出使用说明 PDF](../output/pdf/单输出布尔函数使用说明.pdf) · [多输出使用说明 PDF](../output/pdf/多输出布尔函数使用说明.pdf)。

## 单输出布尔函数

`single-output/` 中的三个文件表示同一个 4 元布尔函数：

`f(x1,x2,x3,x4) = x1·x2 ⊕ x3 ⊕ 1`，其中 `x1` 为最高位。真值表按 `0000`、`0001`、…、`1111` 排列。

| 文件 | 界面输入格式 | 内容含义 |
| --- | --- | --- |
| `single-output/quadratic_4var_truth_binary.txt` | 自动识别，或二进制真值表 | `0b` 后是 16 位输出，顺序对应 16 个输入 |
| `single-output/quadratic_4var_truth_hex.txt` | 自动识别，或十六进制真值表 | `0x` 后是 4 位十六进制，逐位展开后同样是 16 位输出 |
| `single-output/quadratic_4var_anf.txt` | 自动识别，或 ANF | `n=4;` 声明变量数，`+` 表示异或，`*` 表示乘积 |

三个文件应得到相同的 `Truth_table.txt`（`1100110011000011`）和相同的规范化 `ANF.txt`。直接选「单函数分析」即可；输入格式保持「自动识别」最方便。分析结果会保存到输入文件旁的 `bf_output`，也可以在界面另选目录。

## 多输出布尔函数

`multi-output/` 中的四个文件都表示同一个 PRESENT 4×4 S 盒。界面设置 **输入位数 `n=4`、输出位数 `m=4`**，`x1` 和 `f1` 均为最高位。

| 文件 | 进制 | 「转置真值表」 |
| --- | --- | --- |
| `multi-output/PRESENT_4x4_truth_hex.txt` | 十六进制 | 不勾选 |
| `multi-output/PRESENT_4x4_truth_binary.txt` | 二进制 | 不勾选 |
| `multi-output/PRESENT_4x4_transposed_hex.txt` | 十六进制 | 勾选 |
| `multi-output/PRESENT_4x4_transposed_binary.txt` | 二进制 | 勾选 |

普通真值表按 `x=0,1,…,15` 给出 16 个输出词；转置真值表每行是一个坐标函数，依次为 `f1` 至 `f4`，每行按相同输入顺序写出 16 个输出位。四个文件的分析结果应一致。多输出页面**只接受真值表输入**，不要把 ANF 当作输入文件。结果默认保存到输入文件旁的 `vbf_output`，也可以另选目录。

两个页面的 TXT 都使用 UTF-8 编码。多输出格式、指标以及输出文件的更多说明见 [多输出使用说明](../multi-output/README.md)；单输出的解析规则见 [项目说明](../README.md#txt-输入约定)。
