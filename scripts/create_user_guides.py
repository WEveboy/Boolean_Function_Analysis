"""Create the two concise, printable Chinese usage guides."""

from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    HRFlowable,
    KeepTogether,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output" / "pdf"
OUT.mkdir(parents=True, exist_ok=True)
pdfmetrics.registerFont(TTFont("Deng", r"C:\Windows\Fonts\Deng.ttf"))
pdfmetrics.registerFont(TTFont("DengBold", r"C:\Windows\Fonts\Dengb.ttf"))

INK = colors.HexColor("#162435")
MUTED = colors.HexColor("#4f6173")
BLUE = colors.HexColor("#2456a6")
PALE = colors.HexColor("#eef4fc")
LINE = colors.HexColor("#dce5ee")

styles = {
    "eyebrow": ParagraphStyle("eyebrow", fontName="DengBold", fontSize=9, leading=12, textColor=BLUE, spaceAfter=5),
    "title": ParagraphStyle("title", fontName="DengBold", fontSize=22, leading=29, textColor=INK, spaceAfter=6),
    "subtitle": ParagraphStyle("subtitle", fontName="Deng", fontSize=10, leading=16, textColor=MUTED, spaceAfter=14),
    "h2": ParagraphStyle("h2", fontName="DengBold", fontSize=12.5, leading=19, textColor=INK, spaceBefore=13, spaceAfter=6),
    "body": ParagraphStyle("body", fontName="Deng", fontSize=9.6, leading=16, textColor=INK, spaceAfter=6),
    "small": ParagraphStyle("small", fontName="Deng", fontSize=8.5, leading=13.5, textColor=MUTED, spaceAfter=5),
    "table_head": ParagraphStyle("table_head", fontName="DengBold", fontSize=8.4, leading=12, textColor=INK),
    "table": ParagraphStyle("table", fontName="Deng", fontSize=8.2, leading=12.5, textColor=INK),
    "code": ParagraphStyle("code", fontName="Courier", fontSize=8.6, leading=13, textColor=INK),
}


def p(text, style="body"):
    return Paragraph(text, styles[style])


def section(title):
    return p(title, "h2")


def code(lines):
    text = "<br/>".join(escape(line).replace(" ", "&nbsp;") for line in lines)
    table = Table([[p(text, "code")]], colWidths=[176 * mm])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), PALE),
        ("BOX", (0, 0), (-1, -1), 0.5, LINE),
        ("LEFTPADDING", (0, 0), (-1, -1), 12),
        ("RIGHTPADDING", (0, 0), (-1, -1), 12),
        ("TOPPADDING", (0, 0), (-1, -1), 9),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 9),
    ]))
    return table


def table(headers, rows, widths):
    data = [[p(escape(h), "table_head") for h in headers]]
    data.extend([[p(escape(str(value)), "table") for value in row] for row in rows])
    obj = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    obj.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), PALE),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#fafcff")]),
        ("LINEBELOW", (0, 0), (-1, 0), 0.7, LINE),
        ("LINEBELOW", (0, 1), (-1, -1), 0.35, LINE),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 9),
        ("RIGHTPADDING", (0, 0), (-1, -1), 9),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return obj


def decorate(canvas, doc):
    canvas.saveState()
    width, height = A4
    canvas.setFillColor(BLUE)
    canvas.rect(0, height - 6 * mm, width, 6 * mm, stroke=0, fill=1)
    canvas.setStrokeColor(LINE)
    canvas.line(17 * mm, 18 * mm, width - 17 * mm, 18 * mm)
    canvas.setFont("Deng", 8)
    canvas.setFillColor(MUTED)
    canvas.drawString(17 * mm, 12.5 * mm, "布尔函数安全性分析 · v2.0.0")
    canvas.drawRightString(width - 17 * mm, 12.5 * mm, str(doc.page))
    canvas.restoreState()


def build(name, flow):
    target = OUT / name
    doc = SimpleDocTemplate(
        str(target), pagesize=A4, leftMargin=17 * mm, rightMargin=17 * mm,
        topMargin=19 * mm, bottomMargin=24 * mm,
        title=name.removesuffix(".pdf"), author="Boolean Function Analysis",
    )
    doc.build(flow, onFirstPage=decorate, onLaterPages=decorate)
    print(f"Created {target} ({target.stat().st_size} bytes)")


single = [
    p("USER GUIDE  /  SINGLE OUTPUT", "eyebrow"),
    p("单输出布尔函数 · 使用说明", "title"),
    p("从 TXT 导入一个布尔函数，计算所选安全性指标，并导出标准化真值表与 ANF。", "subtitle"),
    HRFlowable(width="100%", thickness=0.8, color=LINE),
    section("01  准备并导入 TXT"),
    p("运行 Windows 便携版 EXE，在左侧选择“单输出布尔函数”，保留“单函数分析”。将范例 TXT 拖入导入区，或点击“浏览”。三个范例表示同一个函数，可任选其一。"),
    table(["范例 TXT", "文件内容 / 选择"], [
        ("quadratic_4var_truth_binary.txt", "0b1100110011000011；输入类型与真值表进制均可选“自动识别”。"),
        ("quadratic_4var_truth_hex.txt", "0xCCC3；输入类型与真值表进制均可选“自动识别”。"),
        ("quadratic_4var_anf.txt", "n=4; x1*x2 + x3 + 1；输入类型选“自动识别”或“代数正规型”。"),
    ], [77 * mm, 99 * mm]),
    section("02  选择指标并运行"),
    p("“变量数 n”可留空，由文件自动推断为 4。勾选需要的指标，然后点击“开始分析”。不勾选指标也会完成真值表与 ANF 的相互转换。“保存位置”留空时，结果写入输入文件旁的 bf_output 文件夹。"),
    section("03  核对结果"),
    p("三份输入文件对应 f(x1,x2,x3,x4) = x1*x2 + x3 + 1，其中“+”表示异或、" 
      "“*”表示变量乘积，x1 是输入的最高位。输入按 0000、0001、…、1111 排列。"),
    code(["Truth_table.txt: 1100110011000011", "ANF.txt: n=4;  x1*x2 + x3 + 1"]),
    Spacer(1, 6),
    p("实际导出的 ANF 排版可由程序规范化；数学表达式与上例等价。页面显示指标值，并始终生成 Truth_table.txt 和 ANF.txt；重复运行会新建 run-2、run-3 等子目录，避免覆盖。", "small"),
    section("输入格式要点"),
    p("真值表展开后必须恰好有 2^n 位。二进制建议加 0b 前缀；十六进制建议加 0x 前缀，保留前导 0。纯 0/1 无前缀内容默认按二进制读取。ANF 可写 n=变量数; 后接异或项；TXT 使用 UTF-8，不要写注释或说明文字。"),
    p("如需双函数互相关，切换到“双函数互相关”并导入两份进制相同、长度相同的真值表；ANF 范例用于单函数分析。", "small"),
]

multi = [
    p("USER GUIDE  /  MULTI OUTPUT", "eyebrow"),
    p("多输出布尔函数 · 使用说明", "title"),
    p("导入向量函数或 S 盒的真值表，按需计算安全性指标、导出 DDT 和 LAT。", "subtitle"),
    HRFlowable(width="100%", thickness=0.8, color=LINE),
    section("01  选择范例和导入设置"),
    p("运行 Windows 便携版 EXE，在左侧选择“多输出布尔函数”。在导入区拖入 TXT 或点击“浏览文件”。下列四份文件表示同一个 PRESENT 4×4 S 盒：输入位数 n=4、输出位数 m=4。"),
    table(["范例 TXT", "进制", "转置真值表"], [
        ("PRESENT_4x4_truth_hex.txt", "十六进制", "不勾选"),
        ("PRESENT_4x4_truth_binary.txt", "二进制", "不勾选"),
        ("PRESENT_4x4_transposed_hex.txt", "十六进制", "勾选"),
        ("PRESENT_4x4_transposed_binary.txt", "二进制", "勾选"),
    ], [106 * mm, 35 * mm, 35 * mm]),
    section("02  理解两种真值表"),
    p("普通格式按 x=0、1、…、15 的顺序给出 16 个输出词，x1 和 f1 为最高位。十六进制范例为："),
    code(["C 5 6 B 9 0 A D 3 E F 8 4 7 1 2"]),
    Spacer(1, 5),
    p("转置格式每行对应一个坐标函数，依次为 f1、f2、f3、f4；行内仍按 x=0、1、…、15 排列。转置十六进制范例为："),
    code(["9B70", "E16C", "32E5", "59A6"]),
    section("03  选择指标并运行"),
    p("勾选所需指标；如需完整表，另勾选“差分分布表 DDT.csv”或“线性近似表 LAT.csv”。点击“开始分析”。“输出位置”留空时，文件写入输入文件旁的 vbf_output 文件夹。"),
    section("04  查看导出文件"),
    p("指标值显示在页面。无论是否勾选指标，都会生成标准化 Truth_table.txt 和坐标函数 ANF.txt；只有主动勾选完整表时才生成 DDT.csv / LAT.csv。四份范例的结果应一致。目标目录非空时，程序新建 run-2、run-3 等子目录。"),
    p("TXT 使用 UTF-8，不能含注释。普通格式须有 2^n 个定宽输出词：二进制每词 m 位，十六进制每词 ceil(m/4) 位。转置格式须有 m 行，每行 2^n 个输出位。多输出页面只接受真值表，不接受 ANF 作为输入。", "small"),
]

build("单输出布尔函数使用说明.pdf", single)
build("多输出布尔函数使用说明.pdf", multi)
