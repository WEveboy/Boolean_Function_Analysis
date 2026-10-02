"""Build the XMind map for section 2.1 of Zhang Weiguo's Boolean-function chapter."""

from __future__ import annotations

import json
import uuid
import zipfile
from pathlib import Path


OUT = Path(__file__).resolve().parents[1] / "docs" / "design" / "单输出布尔函数安全性指标.xmind"


def topic(title: str, page: str = "", detail: str = "", children: list | None = None) -> dict:
    data = {"id": uuid.uuid4().hex, "class": "topic", "title": title}
    if page or detail:
        source = f"张卫国《密码函数》第2章 2.1节，书页 {page}。" if page else ""
        data["notes"] = {"plain": {"content": (source + "\n" + detail).strip()}}
    if children:
        data["children"] = {"attached": children}
    return data


T = topic
root = T("单输出布尔函数安全性指标｜2.1 布尔函数及其密码学性质", "51–63", children=[
    T("基础对象与表示", "51–55", children=[
        T("n元布尔函数 f:F₂ⁿ→F₂", "52", "输入 n 比特，输出 1 比特。Bₙ 是全部 n 元布尔函数的集合。"),
        T("真值表与输入次序", "52–53", "按输入二进制值从 0 到 2ⁿ−1 排列；x₁ 是最高位。真值表有 2ⁿ 位；支撑集 supp(f)={x|f(x)=1}。"),
        T("代数正规型 ANF", "53–55", "f(x)=⊕_{I⊆{1,…,n}} λ_I∏_{i∈I}x_i，λ_I∈F₂。系数与真值表一一对应；由 Möbius 变换计算。"),
        T("仿射函数集合 Aₙ", "55", "l(x)=α·x⊕ε。线性函数对应 ε=0；代数次数≤1 的函数均为仿射函数。"),
        T("共同谱工具：Walsh 变换", "56–57", "W_f(α)=Σ_x(−1)^{f(x)⊕α·x}。频谱按 α 的二进制值排序，可由快速 Walsh–Hadamard 变换计算。"),
        T("共同差分工具：自相关", "62", "C_f(a)=Σ_x(−1)^{f(x)⊕f(x⊕a)}。C_f(0)=2ⁿ。"),
    ]),
    T("平衡性｜输出分布", "53,57", children=[
        T("判定", "53", "wt(f)=|supp(f)|=2ⁿ⁻¹，即输出 0 和 1 各占一半。"),
        T("Walsh 等价条件", "57", "f 平衡 ⇔ W_f(0)=0；并且 W_f(0)=2ⁿ−2wt(f)。"),
        T("安全含义", "53", "平衡性是设计密码函数的基本要求之一。"),
    ]),
    T("代数次数｜高阶非线性", "55", children=[
        T("定义 deg(f)", "55", "ANF 中非零单项式的最大变量个数：deg(f)=max{|I|:λ_I≠0}。零函数的次数需在实现中单独约定。"),
        T("仿射 / 非线性分界", "55", "deg(f)≤1 为仿射；非仿射函数为非线性布尔函数。"),
        T("设计指向", "55", "较高代数次数是优良密码函数的基本要求之一。"),
    ]),
    T("非线性度｜抵抗线性逼近", "55–61", children=[
        T("定义 N_f", "55", "N_f=min_{l∈Aₙ} d_H(f,l)，即与所有仿射函数的最小汉明距离。"),
        T("Walsh 快速计算", "57–58", "N_f=2ⁿ⁻¹−½max_α|W_f(α)|；谱值绝对值越小，非线性度越高。"),
        T("Parseval 恒等式", "57", "Σ_α W_f(α)²=2²ⁿ，可用于频谱计算自检。"),
        T("bent 函数", "58", "仅偶数 n 存在；对所有 α，W_f(α)=±2ⁿᐟ²；达到最大非线性度，但 n≥4 时次数≤n/2，且不平衡。"),
        T("plateaued / 半 bent", "59–60", "plateaued 的 Walsh 值仅取 0、±2ˢ。半 bent：奇数 n 为 0、±2⁽ⁿ⁺¹⁾ᐟ²；偶数 n 为 0、±2ⁿᐟ²⁺¹。"),
        T("几乎最优与严格几乎最优", "60", "几乎最优：奇数 n 时 max|W_f|≤2^((n+1)/2)，偶数 n 时 ≤2^(n/2+1)。严格几乎最优：非线性度严格大于相应的 2^(n−1)−2^((n−1)/2) 或 2^(n−1)−2^(n/2)。"),
    ]),
    T("相关免疫性与弹性｜抵抗相关攻击", "58–61", children=[
        T("相关免疫 t 阶", "58–59", "Xiao–Massey：对 1≤wt(α)≤t 的全部 α，有 W_f(α)=0。等价于对应 Fourier 系数为零。"),
        T("弹性 t 阶", "59", "平衡且 t 阶相关免疫；等价于对 0≤wt(α)≤t 的全部 α，有 W_f(α)=0。"),
        T("Siegenthaler 不等式", "59", "非线性 t 阶弹性函数满足 deg(f)≤n−t−1；提高弹性阶可能限制代数次数。"),
        T("非线性度权衡", "59–61", "低重量处的 Walsh 零点越多，频谱的其余点受 Parseval 约束，非线性度也受到限制。"),
    ]),
    T("代数免疫性｜抵抗代数攻击", "61–62", children=[
        T("零化子", "61", "非零 g 若满足 f·g=0，则 g 是 f 的零化子；同时考虑 f⊕1 的零化子。"),
        T("代数免疫阶 AI(f)", "61", "AI(f)=min{deg(g):g≠0 且 fg=0 或 (f⊕1)g=0}，上界为 ⌈n/2⌉。"),
        T("快速代数免疫阶 FAI(f)", "62", "考察低次数 g 及非零 h=f·g，寻找 deg(g)+deg(h) 的小值；原书将 FAI(f)=n−1 称为次最优。"),
        T("计算方法边界", "61–62", "AI 可用低次数多项式系数的 GF(2) 线性方程求解；FAI 的精确搜索规模更大，需限定 n。"),
    ]),
    T("自相关与雪崩｜输入变化扩散", "62–63", children=[
        T("自相关 C_f(a)", "62", "C_f(a)=Σ_x(−1)^{f(x)⊕f(x⊕a)}；|C_f(a)|=2ⁿ 表示线性结构。"),
        T("扩散准则 PC(k)", "62", "对全部 1≤wt(a)≤k 的非零 a，有 C_f(a)=0。"),
        T("严格雪崩准则 SAC", "62", "SAC=PC(1)：翻转任一单个输入位，输出变化的概率为 1/2。"),
        T("全局雪崩特征 GAC", "62", "绝对值指标 Δ_f=max_{a≠0}|C_f(a)|；平方和指标 σ_f=Σ_a C_f(a)²。两者越小越好。"),
        T("Walsh 与自相关的关系", "63", "W_f(ω)²=Σ_a C_f(a)(−1)^{ω·a}；Σ_ω W_f(ω)^4=2ⁿσ_f。"),
    ]),
    T("指标关系与等价变换", "58–63", children=[
        T("目标不是逐项独立最大化", "58–61", "bent 的高非线性度与平衡性、弹性及次数存在权衡；弹性阶与次数、非线性度也存在限制。"),
        T("仿射输入变换", "63", "f′(x)=f(xA⊕a)，其中 A 可逆。"),
        T("EA 变换", "63", "f″(x)=f(xA⊕a)⊕β·x⊕ε。"),
        T("EA 下可靠的不变量", "63", "非线性度及 GAC 的 Δ、σ 在 EA 下不变；非仿射函数的次数也不变。仅输入仿射变换时，AI/FAI 不变。"),
        T("原书引理 2.20 的适用提醒", "63", "原书把次数、AI、FAI 均列为一般 EA 不变量，需谨慎。反例：f=x₁x₂（3元）与 f⊕x₃ 是 EA 等价的，但 AI 分别为 1、2；仿射函数加线性项也可能改变次数。实现中须重新计算这些指标。"),
        T("验证关系", "57,62–63", "实现自检：W(0)=2ⁿ−2wt(f)、Parseval、C(0)=2ⁿ、W²=FWHT(C)、ΣW⁴=2ⁿσ。"),
    ]),
])

root["structureClass"] = "org.xmind.ui.map.unbalanced"
sheet = {
    "id": uuid.uuid4().hex,
    "class": "sheet",
    "title": "2.1 单输出布尔函数安全性指标",
    "rootTopic": root,
}
metadata = {
    "creator": {"name": "Codex", "version": "1.0"},
    "dataStructureVersion": "3",
    "layoutEngineVersion": "5",
}
manifest = {"file-entries": {"content.json": {}, "metadata.json": {}}}

with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as zf:
    zf.writestr("content.json", json.dumps([sheet], ensure_ascii=False, indent=2))
    zf.writestr("metadata.json", json.dumps(metadata, ensure_ascii=False, indent=2))
    zf.writestr("manifest.json", json.dumps(manifest, ensure_ascii=False, indent=2))

print(OUT)
