"""Build the editable XMind map for section 3.1 of Zhang Weiguo's book."""

from __future__ import annotations

import json
import uuid
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "多输出布尔函数安全性指标.xmind"


def topic(title: str, pages: str = "", detail: str = "", children: list | None = None) -> dict:
    value = {"id": uuid.uuid4().hex, "class": "topic", "title": title}
    if pages or detail:
        source = f"来源：张卫国《密码函数》3.1 节，书页 {pages}。" if pages else ""
        value["notes"] = {"plain": {"content": (source + "\n" + detail).strip()}}
    if children:
        value["children"] = {"attached": children}
    return value


T = topic
root = T("多输出布尔函数安全性指标｜3.1", "166–171", children=[
    T("研究对象与共同计算底座", "166", children=[
        T("(n,m) 函数 F:F₂ⁿ→F₂ᵐ", "166", "n 位输入、m 位输出；二进制实现时 F(X)=(f₁(X),…,fₘ(X))。约定输出 f₁ 为最高位。"),
        T("非零输出掩码 c∈F₂ᵐ\\{0}", "166", "共有 2ᵐ−1 个；分量 f_c(X)=c·F(X)=⊕ᵢcᵢfᵢ(X)。所有非零线性组合都必须纳入分量型指标。"),
        T("真值表与 ANF", "166,168", "向量真值表包含 2ⁿ 个 m 位输出。每个坐标及非零分量都可用快速 Möbius 变换获得 ANF 与次数。"),
        T("Walsh 谱", "166–168", "W_{f_c}(a)=Σ_X(−1)^{f_c(X)⊕a·X}。弹性与非线性度请求可复用各分量的 Walsh 谱；单独求平衡时直接统计输出频数。"),
        T("差分分布表 DDT", "169", "DDT[a][b]=#{X:F(X⊕a)⊕F(X)=b}。仅 a≠0 参与差分均匀度及差分统计；可逐行生成，不默认保留全表。"),
    ]),
    T("平衡性｜输出均匀", "166–167", children=[
        T("定义", "166", "每个 β∈F₂ᵐ 的原像数均为 2ⁿ⁻ᵐ；需 n≥m。n=m 时即置换函数。"),
        T("分量等价判据", "166–167", "F 平衡 ⇔ 每个 c≠0 的 f_c 平衡 ⇔ W_{f_c}(0)=0。只检查 m 个坐标不够；书页 167 的例 3.2 逐一核查了其余线性组合。"),
        T("程序输出", "166", "输出直方图、目标频数、是否平衡、未满足的 β 或 c；避免仅给布尔结论。"),
    ]),
    T("t 阶弹性｜抗相关攻击", "167–168", children=[
        T("定义 3.3", "167", "固定任意 t 个输入位后，每个输出 β 出现 2ⁿ⁻ᵐ⁻ᵗ 次；t=0 对应平衡。必要条件 t≤n−m。"),
        T("分量 Walsh 判据｜引理 3.4", "168", "对所有 c≠0、所有 wt(a)≤t，有 W_{f_c}(a)=0；包含 a=0，故自动要求平衡。"),
        T("弹性阶", "167–168", "从 t=0 开始查找最大满足阶；若连平衡也不满足，报告“不具备 0 阶弹性”。"),
    ]),
    T("代数指标｜全部非零分量", "168", children=[
        T("最大代数次数 Deg(F)", "168", "Deg(F)=max_{c≠0} deg(f_c)。由于坐标自身也属于分量，可由坐标 ANF 的最高次数计算最大值；仍可用分量扫描核验。"),
        T("最小代数次数 deg(F)", "168", "deg(F)=min_{c≠0} deg(f_c)；必须检查非零线性组合，因为高次项可能抵消。零分量的零多项式次数使用明确约定。"),
        T("代数免疫阶 AI(F)", "168", "AI(F)=min_{c≠0} AI(f_c)；AI(f_c) 同时考虑 f_c 和 1⊕f_c 的非零零化子。精确求解可转为 GF(2) 线性方程。"),
    ]),
    T("非线性度与谱分类", "168–169", children=[
        T("向量非线性度 N_F", "168", "N_F=min_{c≠0} N_{f_c}，其中 N_{f_c}=2ⁿ⁻¹−½max_a|W_{f_c}(a)|。必须报告最弱分量及其掩码。"),
        T("几乎最优 / 严格几乎最优", "168", "书中式 (3.6)：奇数 n 的门槛 2ⁿ⁻¹−2⁽ⁿ⁻¹⁾ᐟ²，偶数 n 的门槛 2ⁿ⁻¹−2ⁿᐟ²；超过门槛称严格几乎最优。"),
        T("多输出 bent", "168", "仅偶数 n 且 m≤n/2 时可能达到 N_F=2ⁿ⁻¹−2ⁿᐟ²⁻¹；等价于每个非零分量都是 bent。此类函数不平衡。"),
        T("AB：几乎 bent", "169", "当 n=m≥3 为奇数、F 为 APN 且 N_F=2ⁿ⁻¹−2⁽ⁿ⁻¹⁾ᐟ² 时，书中称 AB 函数；不要把“APN”单独等同“AB”。"),
    ]),
    T("差分均匀性｜抗差分攻击", "169–170", children=[
        T("差分计数 δ(a,b)", "169", "δ(a,b)=#{X:F(X⊕a)⊕F(X)=b}；每个 a≠0 的 DDT 行之和为 2ⁿ，理想平均为 2ⁿ⁻ᵐ。"),
        T("差分均匀度 δ_F", "169", "δ_F=max_{a≠0,b}δ(a,b)。数值越小，最大差分概率 δ_F/2ⁿ 越低。"),
        T("PN / APN", "169", "PN：δ_F=2ⁿ⁻ᵐ。n=m 时 APN：δ_F=2。应检查参数条件；APN 不意味着 AB。"),
        T("最大偏差 δ(F)", "169", "δ(F)=max_{a≠0,b}|δ(a,b)−2ⁿ⁻ᵐ|。与 δ_F 不同：前者衡量偏离平均，后者取 DDT 最大计数。"),
        T("差分标准差 sd(F)", "169", "sd(F)=sqrt[Σ_{a≠0,b}(δ(a,b)−2ⁿ⁻ᵐ)² / (2ᵐ(2ⁿ−1))]。仅在 n、m 相同的函数间直接比较；PN 为 0。"),
        T("已知函数例子", "169–170", "书中举 x³ 与 x⁻¹ 的低差分幂函数：奇数 n 的 x³ 是 APN；偶数 n≥4 的 x⁻¹ 是 4 差分。可用小规模向量验证。"),
    ]),
    T("等价变换与指标核验", "170–171", children=[
        T("仿射等价", "170–171", "F′=L₂∘F∘L₁，其中 L₁、L₂ 为可逆仿射置换。"),
        T("EA 等价", "171", "F′=L₂∘F∘L₁⊕L₃，L₃ 为 (n,m) 仿射函数。书中列出代数次数、非线性度、差分均匀度为不变量；实现宜对低次数退化情形重新计算次数。"),
        T("CCZ 等价", "171", "把图像集合 {(X,F(X))} 经 F₂ⁿ⁺ᵐ 上仿射置换映成另一函数的图像。书中明确非线性度与差分均匀度保持。"),
        T("交叉校验", "166–169", "输出直方图 ↔ 全部 f_c 的 W(0)；每个分量的 Walsh 变换满足 Parseval；每行 DDT 总和为 2ⁿ；DDT[a][b] 在 a≠0 时为偶数。"),
    ]),
    T("C++ 按需调用设计", detail="对应同目录的《多输出布尔函数_C++功能实现文件大纲.md》；此分支是实现设计，而非书中的新指标。", children=[
        T("独立指标入口", detail="balance、is_t_resilient、resiliency_order、maximum_degree、minimum_degree、algebraic_immunity、nonlinearity、differential_uniformity、maximum_differential_deviation、differential_standard_deviation 各自按需调用。"),
        T("仅计算真实依赖", detail="平衡→输出直方图；次数→分量 ANF；AI→GF(2) 零化子求解；弹性/非线性度→所需 Walsh；差分指标→逐行 DDT。互不触发无关模块。"),
        T("批量请求复用", detail="相同分量的 ANF/Walsh 只计算一次；多个差分指标共享同一遍 DDT 行扫描，但只启用被请求的聚合公式。"),
        T("AB 显式组合", detail="仅请求 AB 时组合 APN 所需 δ_F 与非线性度 N_F；只请求 N_F 不运行 DDT。"),
        T("验证无关计算未执行", detail="用调用计数或注入式计算探针检查单指标请求；检查批量请求共享变换与差分扫描。"),
    ]),
])

root["structureClass"] = "org.xmind.ui.map.unbalanced"
sheet = {"id": uuid.uuid4().hex, "class": "sheet", "title": "3.1 多输出布尔函数安全性指标", "rootTopic": root}
metadata = {"creator": {"name": "Codex", "version": "1.0"}, "dataStructureVersion": "3", "layoutEngineVersion": "5"}
manifest = {"file-entries": {"content.json": {}, "metadata.json": {}}}

with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("content.json", json.dumps([sheet], ensure_ascii=False, indent=2))
    archive.writestr("metadata.json", json.dumps(metadata, ensure_ascii=False, indent=2))
    archive.writestr("manifest.json", json.dumps(manifest, ensure_ascii=False, indent=2))

print(OUT)
