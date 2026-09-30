#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cwctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

static std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), size, nullptr, nullptr);
    return out;
}
static std::string json_string(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32) { char b[7]; snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += char(c);
        }
    }
    return out + '"';
}
static std::string normalize_text(const std::string& input) {
    if (input.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), int(input.size()), nullptr, 0);
    if (!length) throw std::runtime_error("TXT 文件必须使用有效的 UTF-8 编码。");
    std::wstring wide(size_t(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), int(input.size()), wide.data(), length))
        throw std::runtime_error("TXT 文件必须使用有效的 UTF-8 编码。");
    std::wstring filtered;
    filtered.reserve(wide.size());
    for (wchar_t c : wide) {
        unsigned code = unsigned(c);
        bool space = code <= 0x20 || code == 0x0085 || code == 0x00A0 ||
                     code == 0x00AD || code == 0x1680 || code == 0x180E ||
                     (code >= 0x2000 && code <= 0x200F) ||
                     (code >= 0x2028 && code <= 0x202F) || code == 0x205F ||
                     (code >= 0x2060 && code <= 0x206F) || code == 0x3000 ||
                     code == 0xFEFF;
        if (!space) filtered += c;
    }
    return utf8(filtered);
}
static bool pow2(size_t x) { return x >= 2 && (x & (x - 1)) == 0; }
static int bits_for_size(size_t x) { return int(std::bit_width(x) - 1); }
static int pop(uint32_t x) { return std::popcount(x); }

struct FunctionData {
    int n = 0;
    std::vector<uint8_t> truth;
    std::vector<uint8_t> anf;
    std::vector<uint64_t> packed_truth;
    std::string kind;
    std::string radix;
    fs::path source;
};
static void pack_truth(FunctionData& f) {
    f.packed_truth.assign((f.truth.size() + 63) / 64, 0);
    for (size_t i = 0; i < f.truth.size(); ++i) if (f.truth[i]) f.packed_truth[i / 64] |= uint64_t(1) << (i % 64);
}

static std::string read_utf8(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("无法读取文件：" + utf8(path.wstring()));
    in.seekg(0, std::ios::end);
    auto size = in.tellg();
    if (size < 0 || size > 8 * 1024 * 1024) throw std::runtime_error("TXT 文件过大，最大支持 8 MB。");
    in.seekg(0);
    std::string data(size_t(size), '\0');
    in.read(data.data(), size);
    if (data.size() >= 3 && uint8_t(data[0]) == 0xEF && uint8_t(data[1]) == 0xBB && uint8_t(data[2]) == 0xBF)
        data.erase(0, 3);
    return data;
}

static void mobius(std::vector<uint8_t>& a, int n) {
    const size_t N = a.size();
    for (int b = 0; b < n; ++b) {
        const size_t step = size_t(1) << b;
        for (size_t base = 0; base < N; base += step * 2)
            for (size_t j = 0; j < step; ++j) a[base + step + j] ^= a[base + j];
    }
}

static int parse_positive(const std::string& s, const char* field) {
    if (s.empty() || !std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); }))
        throw std::runtime_error(std::string(field) + " 必须是正整数。");
    try { int v = std::stoi(s); if (v > 0) return v; } catch (...) {}
    throw std::runtime_error(std::string(field) + " 必须是正整数。");
}

static FunctionData parse_truth(const fs::path& path, std::string clean, const std::string& forced_radix, int requested_n) {
    std::string prefix;
    if (clean.size() >= 2 && clean[0] == '0' && (clean[1] == 'b' || clean[1] == 'B' || clean[1] == 'x' || clean[1] == 'X')) {
        prefix = (clean[1] == 'b' || clean[1] == 'B') ? "bin" : "hex";
        clean.erase(0, 2);
    }
    std::string radix = prefix;
    if (forced_radix != "auto") {
        if (!prefix.empty() && forced_radix != prefix) throw std::runtime_error("文件进制前缀与界面选择不一致。");
        radix = forced_radix;
    }
    if (radix.empty()) {
        radix = "bin";
        for (char c : clean) if (c != '0' && c != '1') { radix = "hex"; break; }
    }
    if (clean.empty()) throw std::runtime_error("真值表 TXT 为空。");
    FunctionData f; f.kind = "truth"; f.radix = radix; f.source = path;
    if (radix == "bin") {
        for (size_t i = 0; i < clean.size(); ++i) {
            if (clean[i] != '0' && clean[i] != '1')
                throw std::runtime_error("二进制真值表第 " + std::to_string(i + 1) + " 个字符无效。");
            f.truth.push_back(uint8_t(clean[i] - '0'));
        }
    } else if (radix == "hex") {
        for (size_t i = 0; i < clean.size(); ++i) {
            unsigned char c = (unsigned char)clean[i];
            int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (v < 0) throw std::runtime_error("十六进制真值表第 " + std::to_string(i + 1) + " 个字符无效。");
            for (int b = 3; b >= 0; --b) f.truth.push_back(uint8_t((v >> b) & 1));
        }
    } else throw std::runtime_error("不支持的进制。");
    if (!pow2(f.truth.size())) throw std::runtime_error("真值表展开后的长度必须为 2 的正整数次幂，当前为 " + std::to_string(f.truth.size()) + " 位。");
    f.n = bits_for_size(f.truth.size());
    if (f.n > 20) throw std::runtime_error("当前最多支持 20 元布尔函数。");
    if (requested_n && requested_n != f.n) throw std::runtime_error("指定的 n 与真值表长度不一致，检测到 n=" + std::to_string(f.n) + "。");
    f.anf = f.truth;
    mobius(f.anf, f.n);
    pack_truth(f);
    return f;
}

static FunctionData parse_anf(const fs::path& path, std::string clean, int requested_n) {
    const std::string xor_sign = "\xE2\x8A\x95";
    for (size_t p = 0; (p = clean.find(xor_sign, p)) != std::string::npos;) clean.replace(p, xor_sign.size(), "+");
    int header_n = 0;
    if (clean.rfind("n=", 0) == 0) {
        size_t semicolon = clean.find(';');
        if (semicolon == std::string::npos) throw std::runtime_error("ANF 的 n=<值> 后必须有分号。");
        header_n = parse_positive(clean.substr(2, semicolon - 2), "n");
        clean.erase(0, semicolon + 1);
    }
    if (clean.size() >= 2 && (clean[0] == 'f' || clean[0] == 'F') && clean[1] == '(') {
        size_t end = clean.find(")=");
        if (end == std::string::npos) throw std::runtime_error("ANF 函数头应写成 f(xn,...,x1)=。");
        std::string variables = clean.substr(2, end - 2);
        std::set<int> indices;
        size_t start = 0;
        while (start < variables.size()) {
            size_t comma = variables.find(',', start);
            if (comma == std::string::npos) comma = variables.size();
            std::string item = variables.substr(start, comma - start);
            if (item.size() < 2 || (item[0] != 'x' && item[0] != 'X'))
                throw std::runtime_error("ANF 函数头中的变量格式无效。");
            int index = parse_positive(item.substr(1), "函数头变量下标");
            if (!indices.insert(index).second) throw std::runtime_error("ANF 函数头中有重复变量。");
            start = comma + 1;
            if (comma + 1 == variables.size()) throw std::runtime_error("ANF 函数头末尾不能有逗号。");
        }
        int signature_n = int(indices.size());
        if (!signature_n || *indices.begin() != 1 || *indices.rbegin() != signature_n)
            throw std::runtime_error("ANF 函数头须列出 x1 到 xn 的全部变量。");
        if (header_n && header_n != signature_n) throw std::runtime_error("ANF 的 n 声明与函数头变量数不一致。");
        if (requested_n && requested_n != signature_n) throw std::runtime_error("界面指定的 n 与 ANF 函数头变量数不一致。");
        header_n = signature_n;
        clean.erase(0, end + 2);
    }
    if (header_n && requested_n && header_n != requested_n) throw std::runtime_error("ANF 文件声明的 n 与界面指定值不同。");
    if (clean.empty()) throw std::runtime_error("ANF 表达式为空。");
    std::vector<std::vector<int>> terms;
    int max_index = 0;
    size_t start = 0;
    while (start < clean.size()) {
        size_t end = clean.find('+', start);
        if (end == std::string::npos) end = clean.size();
        if (end == start) throw std::runtime_error("ANF 中出现空项或连续加号。");
        std::string term = clean.substr(start, end - start);
        std::vector<int> vars;
        if (term != "0" && term != "1") {
            size_t p = 0;
            while (p < term.size()) {
                if (term[p] != 'x' && term[p] != 'X') throw std::runtime_error("ANF 单项式语法错误：" + term);
                ++p; size_t d = p;
                while (p < term.size() && std::isdigit((unsigned char)term[p])) ++p;
                if (p == d) throw std::runtime_error("ANF 变量缺少下标：" + term);
                int index = parse_positive(term.substr(d, p - d), "变量下标");
                max_index = std::max(max_index, index); vars.push_back(index);
                if (p < term.size() && term[p] == '*') {
                    ++p;
                    if (p == term.size()) throw std::runtime_error("ANF 单项式末尾不能是乘号。");
                }
            }
        }
        if (term == "1") vars.push_back(-1);
        terms.push_back(vars);
        start = end + 1;
        if (end + 1 == clean.size()) throw std::runtime_error("ANF 表达式不能以加号结尾。");
    }
    int n = requested_n ? requested_n : header_n ? header_n : max_index;
    if (n == 0) throw std::runtime_error("常数 ANF 需要在文件中写 n=<值>; 或在界面指定 n。");
    if (n > 20) throw std::runtime_error("当前最多支持 20 元布尔函数。");
    if (max_index > n) throw std::runtime_error("ANF 变量下标超过 n。");
    FunctionData f; f.n = n; f.kind = "anf"; f.radix = "none"; f.source = path;
    f.anf.assign(size_t(1) << n, 0);
    for (const auto& vars : terms) {
        if (vars.empty()) continue;
        uint32_t mask = 0;
        for (int index : vars) if (index > 0) mask |= uint32_t(1) << (n - index);
        f.anf[mask] ^= 1;
    }
    f.truth = f.anf;
    mobius(f.truth, n);
    pack_truth(f);
    return f;
}

static FunctionData load_function(const fs::path& path, const std::string& forced_kind, const std::string& forced_radix, int n) {
    std::wstring ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c){ return wchar_t(std::towlower(c)); });
    if (ext != L".txt") throw std::runtime_error("只能导入 TXT 文件。");
    std::string clean = normalize_text(read_utf8(path));
    if (clean.empty()) throw std::runtime_error("TXT 文件为空。");
    bool truth_prefix = clean.size() >= 2 && clean[0] == '0' && (clean[1] == 'b' || clean[1] == 'B' || clean[1] == 'x' || clean[1] == 'X');
    bool anf_hint = clean.rfind("n=", 0) == 0 || clean.rfind("f(", 0) == 0 || clean.rfind("F(", 0) == 0 || clean.find('+') != std::string::npos || clean.find("\xE2\x8A\x95") != std::string::npos;
    if (!truth_prefix) for (size_t i = 0; i < clean.size(); ++i) if ((clean[i] == 'x' || clean[i] == 'X') && i + 1 < clean.size() && std::isdigit((unsigned char)clean[i + 1])) { anf_hint = true; break; }
    std::string kind = forced_kind == "auto" ? (truth_prefix ? "truth" : anf_hint ? "anf" : "truth") : forced_kind;
    if (kind == "anf") {
        if (truth_prefix) throw std::runtime_error("0b/0x 前缀是真值表格式，不能按 ANF 读取。");
        return parse_anf(path, clean, n);
    }
    if (anf_hint && !truth_prefix) throw std::runtime_error("文件含 ANF 语法，不能按真值表读取。");
    return parse_truth(path, clean, forced_radix, n);
}

static std::string format_anf(const FunctionData& f) {
    std::vector<uint32_t> masks;
    for (uint32_t m = 0; m < f.anf.size(); ++m) if (f.anf[m]) masks.push_back(m);
    std::sort(masks.begin(), masks.end(), [](uint32_t a, uint32_t b){ int da = pop(a), db = pop(b); return da == db ? a > b : da < db; });
    std::string expr;
    for (uint32_t m : masks) {
        if (!expr.empty()) expr += "+";
        if (!m) { expr += "1"; continue; }
        bool first = true;
        for (int i = 1; i <= f.n; ++i) if (m & (uint32_t(1) << (f.n - i))) {
            if (!first) expr += "*";
            expr += "x" + std::to_string(i); first = false;
        }
    }
    return expr.empty() ? "0" : expr;
}

static fs::path available_output_dir(const fs::path& base) {
    fs::create_directories(base);
    if (fs::directory_iterator(base) == fs::directory_iterator()) return base;
    for (int i = 2; i < 10000; ++i) {
        fs::path trial = base / (L"run-" + std::to_wstring(i));
        if (!fs::exists(trial)) { fs::create_directory(trial); return trial; }
    }
    throw std::runtime_error("输出目录重名次数过多。");
}
static void atomic_write(const fs::path& target, const std::string& text) {
    fs::path temp = target; temp += L".tmp";
    if (fs::exists(temp)) fs::remove(temp);
    try {
        { std::ofstream out(temp, std::ios::binary); if (!out) throw std::runtime_error("无法创建输出文件。"); out.write(text.data(), std::streamsize(text.size())); if (!out) throw std::runtime_error("写入输出文件失败。"); }
        fs::rename(temp, target);
    } catch (...) { if (fs::exists(temp)) fs::remove(temp); throw; }
}
static std::vector<fs::path> export_normalized(const FunctionData& f, const fs::path& base) {
    fs::path dir = available_output_dir(base);
    std::vector<fs::path> created;
    try {
        fs::path truth_path = dir / L"Truth_table.txt";
        fs::path anf_path = dir / L"ANF.txt";
        std::string bits; bits.reserve(f.truth.size() + 1);
        for (uint8_t b : f.truth) bits += char('0' + b);
        bits += '\n';
        atomic_write(truth_path, bits); created.push_back(truth_path);
        atomic_write(anf_path, "n=" + std::to_string(f.n) + ";\n" + format_anf(f) + "\n"); created.push_back(anf_path);
    } catch (...) { for (const auto& path : created) { std::error_code ec; fs::remove(path, ec); } throw; }
    return created;
}

template<typename T> static void fwht(std::vector<T>& a) {
    for (size_t len = 1; len < a.size(); len *= 2)
        for (size_t i = 0; i < a.size(); i += len * 2)
            for (size_t j = 0; j < len; ++j) { T x = a[i+j], y = a[i+j+len]; a[i+j] = x+y; a[i+j+len] = x-y; }
}
class Session {
public:
    explicit Session(const FunctionData& f): f(f) {}
    const std::vector<int32_t>& walsh() {
        if (!W) {
            std::vector<int32_t> a(f.truth.size());
            for (size_t i = 0; i < a.size(); ++i) a[i] = f.truth[i] ? -1 : 1;
            fwht(a); W = std::move(a);
        }
        return *W;
    }
    const std::vector<int32_t>& autocorr() {
        if (!C) {
            const auto& w = walsh();
            std::vector<int64_t> a(w.size());
            for (size_t i = 0; i < w.size(); ++i) a[i] = int64_t(w[i]) * w[i];
            fwht(a);
            std::vector<int32_t> c(a.size());
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i] % int64_t(a.size())) throw std::runtime_error("自相关变换整除校验失败。");
                c[i] = int32_t(a[i] / int64_t(a.size()));
            }
            C = std::move(c);
        }
        return *C;
    }
    bool has_walsh() const { return W.has_value(); }
    const FunctionData& f;
private:
    std::optional<std::vector<int32_t>> W, C;
};

static std::vector<int32_t> crosscorr(Session& sf, Session& sg) {
    const auto& f = sf.walsh(); const auto& g = sg.walsh();
    if (f.size() != g.size()) throw std::runtime_error("互相关输入长度不同。");
    std::vector<int64_t> a(f.size());
    for (size_t i = 0; i < f.size(); ++i) a[i] = int64_t(f[i]) * g[i];
    fwht(a);
    std::vector<int32_t> out(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] % int64_t(a.size())) throw std::runtime_error("互相关变换整除校验失败。");
        out[i] = int32_t(a[i] / int64_t(a.size()));
    }
    return out;
}

static uint32_t binary_word(const std::string& bits, int n, const char* label) {
    if (int(bits.size()) != n) throw std::runtime_error(std::string(label) + " 必须恰好有 n 位。");
    uint32_t value = 0;
    for (char c : bits) { if (c != '0' && c != '1') throw std::runtime_error(std::string(label) + " 只能包含 0/1。"); value = (value << 1) | uint32_t(c - '0'); }
    return value;
}
static FunctionData ea_transform(const FunctionData& f, const std::string& matrix, const std::string& alpha_bits,
                                 const std::string& beta_bits, int epsilon) {
    if (epsilon != 0 && epsilon != 1) throw std::runtime_error("EA 的 ε 必须为 0 或 1。");
    std::vector<uint32_t> rows;
    if (matrix.empty()) {
        for (int i = 0; i < f.n; ++i) rows.push_back(uint32_t(1) << (f.n - i - 1));
    } else {
        std::istringstream in(matrix); std::string row;
        while (std::getline(in, row, ',')) rows.push_back(binary_word(row, f.n, "EA 矩阵行"));
    }
    if (int(rows.size()) != f.n) throw std::runtime_error("EA 矩阵必须有 n 行。");
    auto basis = rows; int rank = 0;
    for (int bit = f.n - 1; bit >= 0; --bit) {
        int pivot = rank;
        while (pivot < f.n && !(basis[pivot] & (uint32_t(1) << bit))) ++pivot;
        if (pivot == f.n) continue;
        std::swap(basis[rank], basis[pivot]);
        for (int r = rank + 1; r < f.n; ++r) if (basis[r] & (uint32_t(1) << bit)) basis[r] ^= basis[rank];
        ++rank;
    }
    if (rank != f.n) throw std::runtime_error("EA 矩阵不可逆，请输入 GF(2) 上的可逆矩阵。");
    uint32_t alpha = alpha_bits.empty() ? 0 : binary_word(alpha_bits, f.n, "EA 平移 α");
    uint32_t beta = beta_bits.empty() ? 0 : binary_word(beta_bits, f.n, "EA 输出线性项 β");
    FunctionData out; out.n = f.n; out.kind = "truth"; out.radix = "bin";
    out.source = f.source.parent_path() / (f.source.stem().wstring() + L".ea.txt");
    out.truth.resize(f.truth.size());
    for (uint32_t x = 0; x < f.truth.size(); ++x) {
        uint32_t y = alpha;
        for (int i = 0; i < f.n; ++i) if (x & (uint32_t(1) << (f.n - i - 1))) y ^= rows[i];
        out.truth[x] = f.truth[y] ^ uint8_t(pop(x & beta) & 1) ^ uint8_t(epsilon);
    }
    out.anf = out.truth; mobius(out.anf, out.n); pack_truth(out);
    return out;
}

struct Distribution {
    std::map<int32_t, uint64_t> counts;
    uint32_t min_abs = UINT32_MAX, max_abs = 0;
    uint32_t first_min = 0, first_max = 0;
};
static Distribution distribution(const std::vector<int32_t>& values, size_t start = 0) {
    Distribution d;
    for (size_t i = start; i < values.size(); ++i) {
        int32_t v = values[i]; ++d.counts[v];
        uint32_t a = uint32_t(std::abs(int64_t(v)));
        if (a < d.min_abs) { d.min_abs = a; d.first_min = uint32_t(i); }
        if (a > d.max_abs) { d.max_abs = a; d.first_max = uint32_t(i); }
    }
    if (start == values.size()) d.min_abs = 0;
    return d;
}
static int32_t autocorr_at(const FunctionData& f, uint32_t mask) {
    if (!mask || mask >= f.truth.size()) throw std::runtime_error("差分下标越界。");
    int32_t sum = 0;
    for (uint32_t x = 0; x < f.truth.size(); ++x) if (x < (x ^ mask)) sum += f.truth[x] == f.truth[x ^ mask] ? 2 : -2;
    return sum;
}
static int degree(const std::vector<uint8_t>& anf) {
    int d = -1;
    for (uint32_t i = 0; i < anf.size(); ++i) if (anf[i]) d = std::max(d, pop(i));
    return d;
}
static uint32_t weight(const FunctionData& f) {
    uint32_t out = 0;
    for (uint64_t word : f.packed_truth) out += std::popcount(word);
    return out;
}
static uint32_t max_abs(const std::vector<int32_t>& x) {
    uint32_t m = 0; for (int32_t a : x) m = std::max(m, uint32_t(std::abs(int64_t(a)))); return m;
}
static int correlation_order(const std::vector<int32_t>& w, int n) {
    int smallest = n + 1;
    for (uint32_t i = 1; i < w.size(); ++i) if (w[i]) smallest = std::min(smallest, pop(i));
    return smallest == n + 1 ? n : smallest - 1;
}
static bool pc_k(Session& s, int k) {
    const auto& f = s.f; const size_t N = f.truth.size();
    if (k < 1 || k > f.n) throw std::runtime_error("PC(k) 的 k 必须在 1 到 n 之间。");
    size_t count = 0;
    for (uint32_t a = 1; a < N; ++a) if (pop(a) <= k) ++count;
    if (s.has_walsh() || count > size_t(2 * f.n)) {
        const auto& c = s.autocorr();
        for (uint32_t a = 1; a < N; ++a) if (pop(a) <= k && c[a] != 0) return false;
    } else {
        for (uint32_t a = 1; a < N; ++a) if (pop(a) <= k && autocorr_at(f, a) != 0) return false;
    }
    return true;
}

static bool has_annihilator(const FunctionData& f, int d, bool complement) {
    std::vector<uint32_t> monomials;
    const uint32_t N = uint32_t(f.truth.size());
    for (uint32_t m = 0; m < N; ++m) if (pop(m) <= d) monomials.push_back(m);
    const size_t cols = monomials.size(), words = (cols + 63) / 64;
    std::vector<std::vector<uint64_t>> basis(cols);
    size_t rank = 0;
    for (uint32_t x = 0; x < N; ++x) {
        bool in_support = complement ? !f.truth[x] : !!f.truth[x];
        if (!in_support) continue;
        std::vector<uint64_t> row(words, 0);
        for (size_t j = 0; j < cols; ++j) if ((monomials[j] & x) == monomials[j]) row[j / 64] |= uint64_t(1) << (j % 64);
        for (size_t j = 0; j < cols; ++j) if (row[j / 64] & (uint64_t(1) << (j % 64))) {
            if (basis[j].empty()) { basis[j] = std::move(row); ++rank; break; }
            for (size_t k = j / 64; k < words; ++k) row[k] ^= basis[j][k];
        }
        if (rank == cols) return false;
    }
    return rank < cols;
}
static int algebraic_immunity(const FunctionData& f) {
    if (f.n > 12) return -1;
    for (int d = 0; d <= (f.n + 1) / 2; ++d)
        if (has_annihilator(f, d, false) || has_annihilator(f, d, true)) return d;
    return -1;
}

static int fast_algebraic_immunity(const FunctionData& f) {
    if (f.n > 6) return -1;
    const int N = int(f.truth.size());
    uint64_t fbits = 0;
    for (int x = 0; x < N; ++x) if (f.truth[x]) fbits |= uint64_t(1) << x;
    std::vector<uint32_t> masks;
    std::vector<uint64_t> monomial_truth;
    std::vector<int> monomial_degree;
    for (uint32_t m = 0; m < uint32_t(N); ++m) if (2 * pop(m) < f.n) {
        masks.push_back(m); monomial_degree.push_back(pop(m));
        uint64_t t = 0;
        for (int x = 0; x < N; ++x) if ((uint32_t(x) & m) == m) t |= uint64_t(1) << x;
        monomial_truth.push_back(t);
    }
    std::vector<uint64_t> lower(f.n);
    for (int b = 0; b < f.n; ++b) for (int x = 0; x < N; ++x) if (!(x & (1 << b))) lower[b] |= uint64_t(1) << x;
    uint64_t limit = uint64_t(1) << masks.size(), previous = 0, gtruth = 0;
    std::vector<int> active(f.n + 1, 0);
    int best = std::numeric_limits<int>::max();
    for (uint64_t i = 1; i < limit; ++i) {
        uint64_t gray = i ^ (i >> 1), changed = gray ^ previous;
        int j = std::countr_zero(changed);
        int d = monomial_degree[j]; active[d] += (gray & changed) ? 1 : -1;
        gtruth ^= monomial_truth[j]; previous = gray;
        uint64_t h = fbits & gtruth;
        if (!h) continue;
        int dg = 0; for (int z = f.n; z >= 0; --z) if (active[z]) { dg = z; break; }
        if (dg >= best) continue;
        uint64_t a = h;
        for (int b = 0; b < f.n; ++b) a ^= (a & lower[b]) << (1 << b);
        int dh = 0; for (int x = 0; x < N; ++x) if (a & (uint64_t(1) << x)) dh = std::max(dh, pop(uint32_t(x)));
        best = std::min(best, dg + dh);
        if (best == 0) break;
    }
    return best == std::numeric_limits<int>::max() ? -2 : best;
}

struct Result {
    std::string id, title, value, detail;
    std::optional<Distribution> dist, nonzero;
};
static std::string bool_cn(bool b) { return b ? "满足" : "不满足"; }
static std::string result_json(const Result& r, size_t max_counts = std::numeric_limits<size_t>::max()) {
    std::string out = "{\"id\":" + json_string(r.id) + ",\"title\":" + json_string(r.title) + ",\"value\":" + json_string(r.value) + ",\"detail\":" + json_string(r.detail);
    if (r.dist) {
        out += ",\"counts\":["; bool first = true;
        size_t listed = 0;
        for (auto [v, count] : r.dist->counts) {
            if (listed++ >= max_counts) break;
            if (!first) out += ','; first = false;
            out += "{\"value\":" + std::to_string(v) + ",\"count\":" + std::to_string(count) + "}";
        }
        out += "],\"countTotal\":" + std::to_string(r.dist->counts.size()) + ",\"stats\":{\"minAbs\":" + std::to_string(r.dist->min_abs) + ",\"maxAbs\":" + std::to_string(r.dist->max_abs)
            + ",\"firstMinIndex\":" + std::to_string(r.dist->first_min) + ",\"firstMaxIndex\":" + std::to_string(r.dist->first_max);
        if (r.nonzero) out += ",\"nonzeroMinAbs\":" + std::to_string(r.nonzero->min_abs) + ",\"nonzeroMaxAbs\":" + std::to_string(r.nonzero->max_abs);
        out += "}";
    }
    return out + "}";
}

static Result calculate(const std::string& id, Session& s, int pc_k_value) {
    const auto& f = s.f; const int N = int(f.truth.size());
    if (id == "balance") return {id, "平衡性", bool_cn(weight(f) * 2 == uint32_t(N)), "汉明重量 " + std::to_string(weight(f)) + " / " + std::to_string(N)};
    if (id == "degree") { int d = degree(f.anf); return {id, "代数次数", d < 0 ? "未定义" : std::to_string(d), d < 0 ? "零多项式" : "通过快速 Möbius 变换得到的 ANF 计算"}; }
    if (id == "nonlinearity") { int v = N/2 - int(max_abs(s.walsh()))/2; return {id, "非线性度", std::to_string(v), "根据 Walsh 谱的最大绝对值计算"}; }
    if (id == "walsh_dist") { Result r{id, "频谱次数分布", "已统计", "按带符号 Walsh 谱值计数"}; r.dist = distribution(s.walsh()); return r; }
    if (id == "correlation_immunity") return {id, "相关免疫阶", std::to_string(correlation_order(s.walsh(), f.n)), "检查非零低重量 Walsh 谱点"};
    if (id == "resiliency") { const auto& w = s.walsh(); return {id, "弹性阶", w[0] ? "不适用" : std::to_string(correlation_order(w, f.n)), w[0] ? "函数不平衡" : "平衡且低重量 Walsh 谱点为零"}; }
    if (id == "bent") { bool yes = f.n % 2 == 0; int target = 1 << (f.n/2); for (int32_t v : s.walsh()) if (std::abs(v) != target) { yes = false; break; } return {id, "Bent 判定", bool_cn(yes), yes ? "所有 Walsh 谱值绝对值相等" : "未满足 Bent 频谱条件"}; }
    if (id == "plateaued") { int amplitude = 0; bool yes = true; int zero = 0; for (int32_t v : s.walsh()) { int a = std::abs(v); if (!a) { ++zero; continue; } if (!amplitude) amplitude = a; else if (amplitude != a) yes = false; } bool power = amplitude && !(amplitude & (amplitude - 1)); yes &= power; return {id, "Plateaued 分类", bool_cn(yes), yes ? "非零谱值绝对值=" + std::to_string(amplitude) + "；零谱点=" + std::to_string(zero) : "非零谱值不属于同一幅度"}; }
    if (id == "ai") { int v = algebraic_immunity(f); return {id, "代数免疫阶 AI", v < 0 ? "超出范围" : std::to_string(v), v < 0 ? "精确算法当前支持 n≤12" : "GF(2) 线性方程组精确求解"}; }
    if (id == "fai") { int v = fast_algebraic_immunity(f); return {id, "快速代数免疫阶 FAI", v == -1 ? "超出范围" : v == -2 ? "不适用" : std::to_string(v), v == -1 ? "精确算法当前支持 n≤6" : v == -2 ? "不存在非零乘积 f·g" : "按低次数 g 穷举，计算 deg(g)+deg(fg) 的最小值"}; }
    if (id == "sac") { bool yes = true; for (int b = 0; b < f.n; ++b) if (autocorr_at(f, uint32_t(1) << b)) { yes = false; break; } return {id, "严格雪崩准则 SAC", bool_cn(yes), "仅计算 " + std::to_string(f.n) + " 个单比特差分"}; }
    if (id == "pc") return {id, "扩散准则 PC(" + std::to_string(pc_k_value) + ")", bool_cn(pc_k(s, pc_k_value)), "检查重量不超过 k 的非零差分"};
    if (id == "gac") { const auto& c = s.autocorr(); uint32_t delta = 0; uint64_t sigma = 0; for (size_t i = 0; i < c.size(); ++i) { if (i) delta = std::max(delta, uint32_t(std::abs(int64_t(c[i])))); sigma += uint64_t(int64_t(c[i]) * c[i]); } return {id, "全局雪崩特征 GAC", "Δ=" + std::to_string(delta), "σ=" + std::to_string(sigma) + "；Δ 仅看非零差分，σ 包含零差分"}; }
    if (id == "autocorr_dist") { const auto& c = s.autocorr(); Result r{id, "自相关值分布", "已统计", "包含零差分；附列非零差分绝对值极值"}; r.dist = distribution(c); if (c.size()>1) r.nonzero = distribution(c, 1); return r; }
    throw std::runtime_error("未知指标：" + id);
}

static std::string join_results(const std::vector<Result>& values, size_t max_counts = std::numeric_limits<size_t>::max()) {
    std::string out = "[";
    for (size_t i = 0; i < values.size(); ++i) { if (i) out += ','; out += result_json(values[i], max_counts); }
    return out + "]";
}
static std::string input_json(const FunctionData& f) {
    return "{\"name\":" + json_string(utf8(f.source.filename().wstring())) + ",\"n\":" + std::to_string(f.n) + ",\"kind\":" + json_string(f.kind) + ",\"radix\":" + json_string(f.radix) + ",\"truthLength\":" + std::to_string(f.truth.size()) + "}";
}
static std::string outputs_json(const std::vector<fs::path>& files) {
    std::string out = "[";
    for (size_t i = 0; i < files.size(); ++i) { if (i) out += ','; out += json_string(utf8(files[i].wstring())); }
    return out + "]";
}

int wmain(int argc, wchar_t** argv) {
    try {
        fs::path input, input2, output;
        std::string kind = "auto", radix = "auto", metrics, ea_rows, ea_alpha, ea_beta;
        int n = 0, pc_k_value = 1, ea_epsilon = 0;
        for (int i = 1; i < argc; ++i) {
            std::wstring key = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("命令行参数缺少值。");
            std::wstring val = argv[++i];
            if (key == L"--input") input = val;
            else if (key == L"--input2") input2 = val;
            else if (key == L"--out") output = val;
            else if (key == L"--kind") kind = utf8(val);
            else if (key == L"--radix") radix = utf8(val);
            else if (key == L"--metrics") metrics = utf8(val);
            else if (key == L"--n") n = parse_positive(utf8(val), "n");
            else if (key == L"--pc-k") pc_k_value = parse_positive(utf8(val), "k");
            else if (key == L"--ea-rows") ea_rows = utf8(val);
            else if (key == L"--ea-alpha") ea_alpha = utf8(val);
            else if (key == L"--ea-beta") ea_beta = utf8(val);
            else if (key == L"--ea-epsilon") { std::string v = utf8(val); if (v != "0" && v != "1") throw std::runtime_error("EA 的 ε 必须为 0 或 1。"); ea_epsilon = v[0] - '0'; }
            else throw std::runtime_error("未知命令行参数。");
        }
        if (input.empty()) throw std::runtime_error("缺少输入 TXT 文件。");
        if (kind != "auto" && kind != "truth" && kind != "anf") throw std::runtime_error("输入类型无效。");
        if (radix != "auto" && radix != "bin" && radix != "hex") throw std::runtime_error("进制选项无效。");
        std::vector<FunctionData> functions;
        functions.push_back(load_function(input, input2.empty() ? kind : "truth", radix, n));
        if (!input2.empty()) {
            functions.push_back(load_function(input2, "truth", radix, 0));
            if (functions[0].radix != functions[1].radix) throw std::runtime_error("两份真值表的进制不一致。");
            if (functions[0].truth.size() != functions[1].truth.size()) throw std::runtime_error("两份真值表的长度不一致。");
        }
        bool ea_requested = metrics == "ea" || metrics.rfind("ea,", 0) == 0 || metrics.find(",ea,") != std::string::npos || (metrics.size() >= 3 && metrics.substr(metrics.size() - 3) == ",ea");
        if (ea_requested && functions.size() != 1) throw std::runtime_error("EA 变换仅适用于单函数模式。");
        std::optional<FunctionData> transformed;
        if (ea_requested) transformed = ea_transform(functions[0], ea_rows, ea_alpha, ea_beta, ea_epsilon);
        if (output.empty()) output = input.parent_path() / L"bf_output";
        Session first(functions[0]);
        std::optional<Session> second;
        if (functions.size() == 2) second.emplace(functions[1]);
        std::optional<Session> transformed_session;
        if (transformed) transformed_session.emplace(*transformed);
        std::vector<Result> results;
        std::vector<Result> transformed_results;
        std::istringstream stream(metrics);
        std::string id;
        while (std::getline(stream, id, ',')) {
            if (id.empty()) continue;
            if (id == "crosscorr_dist") {
                if (!second) throw std::runtime_error("互相关需要两份真值表。");
                auto c = crosscorr(first, *second);
                Result r{id, "互相关值分布", "已统计", "C_fg(a)=Σ(-1)^(f(x)⊕g(x⊕a))"};
                r.dist = distribution(c); results.push_back(std::move(r));
            } else if (id == "ea") {
                if (!transformed) throw std::runtime_error("EA 变换未初始化。");
                results.push_back({id, "EA 变换", "已计算", "已校验矩阵可逆；已勾选的其他指标会在变换后重新计算"});
            } else {
                if (second) throw std::runtime_error("双文件模式只支持互相关指标。");
                results.push_back(calculate(id, first, pc_k_value));
                if (transformed_session) transformed_results.push_back(calculate(id, *transformed_session, pc_k_value));
            }
        }
        std::string inputs = "[";
        for (size_t i = 0; i < functions.size(); ++i) { if (i) inputs += ','; inputs += input_json(functions[i]); }
        inputs += "]";
        auto files = export_normalized(functions[0], output);
        std::cout << "{\"ok\":true,\"inputs\":" << inputs << ",\"files\":" << outputs_json(files) << ",\"results\":" << join_results(results) << ",\"transformedResults\":" << join_results(transformed_results) << "}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "{\"ok\":false,\"error\":" << json_string(e.what()) << "}\n";
        return 1;
    }
}
