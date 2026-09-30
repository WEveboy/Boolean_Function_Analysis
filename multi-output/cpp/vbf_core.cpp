#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using U = std::uint32_t;
using Count = std::uint64_t;

static std::string utf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), out.data(), count, nullptr, nullptr);
    return out;
}
static std::string json_string(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 32) { char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
        else out += char(c);
    }
    return out + '"';
}
static int positive_int(const std::string& raw, const char* name) {
    if (raw.empty() || raw.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error(std::string(name) + " 必须为正整数。");
    long long v = std::stoll(raw);
    if (v < 1 || v > 1000000) throw std::runtime_error(std::string(name) + " 超出范围。");
    return int(v);
}
static std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("无法读取输入文件。");
    std::string text((std::istreambuf_iterator<char>(in)), {});
    if (text.size() >= 3 && (unsigned char)text[0] == 0xef && (unsigned char)text[1] == 0xbb && (unsigned char)text[2] == 0xbf) text.erase(0, 3);
    return text;
}
static void write_file(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("无法创建结果文件。");
    out.write(text.data(), std::streamsize(text.size()));
    if (!out) throw std::runtime_error("写入结果文件失败。");
}
static std::string hex_fixed(U value, int bits) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw((bits + 3) / 4) << std::setfill('0') << value;
    return out.str();
}
static std::string bits_fixed(U value, int width) {
    std::string result(width, '0');
    for (int i = 0; i < width; ++i) result[i] = ((value >> (width - 1 - i)) & 1) ? '1' : '0';
    return result;
}
struct Function {
    int n = 0, m = 0;
    std::vector<U> truth;
    fs::path source;
    bool permutation() const {
        if (n != m) return false;
        std::vector<bool> seen(size_t(1) << m, false);
        for (U y : truth) { if (seen[y]) return false; seen[y] = true; }
        return true;
    }
};
static std::vector<std::string> tokenize(const std::string& text) {
    std::string normalized = text;
    for (char& c : normalized) if (c == ',' || c == ';' || c == '|' || c == '\r' || c == '\n' || c == '\t') c = ' ';
    std::istringstream in(normalized);
    std::vector<std::string> tokens;
    for (std::string s; in >> s;) tokens.push_back(s);
    return tokens;
}
static std::vector<std::uint8_t> mobius(std::vector<std::uint8_t> a, int n);
static std::string compact_anf_line(const std::string& line) {
    std::string compact;
    for (unsigned char c : line) if (c != ' ' && c != '\t' && c != '\r' && c != '\n') compact += char(c);
    return compact;
}
static bool looks_like_anf(const fs::path& path) {
    std::istringstream input(read_file(path));
    for (std::string line; std::getline(input, line);) {
        std::string clean = compact_anf_line(line);
        if (clean.empty() || clean.rfind("n=", 0) == 0) continue;
        return clean.size() > 2 && (clean[0] == 'f' || clean[0] == 'F') &&
            std::isdigit((unsigned char)clean[1]) && clean.find('=') != std::string::npos;
    }
    return false;
}
static Function load_anf_function(const fs::path& path, int n_arg, int m_arg) {
    std::istringstream input(read_file(path));
    std::string line, header;
    while (std::getline(input, line)) {
        header = compact_anf_line(line);
        if (!header.empty()) break;
    }
    if (header.rfind("n=", 0) != 0) throw std::runtime_error("多输出 ANF 首行须写 n=<输入位数>; m=<输出位数>。");
    size_t semicolon = header.find(';');
    if (semicolon == std::string::npos || header.compare(semicolon + 1, 2, "m=") != 0)
        throw std::runtime_error("多输出 ANF 首行须写 n=<输入位数>; m=<输出位数>。");
    std::string m_text = header.substr(semicolon + 3);
    if (!m_text.empty() && m_text.back() == ';') m_text.pop_back();
    int n = positive_int(header.substr(2, semicolon - 2), "n");
    int m = positive_int(m_text, "m");
    if (n < 1 || n > 12 || m < 1 || m > std::min(n, 8))
        throw std::runtime_error("ANF 要求 1≤n≤12、1≤m≤min(n,8)。");
    if ((n_arg && n_arg != n) || (m_arg && m_arg != m))
        throw std::runtime_error("界面中的 n、m 与 ANF 文件首行声明不一致。");
    const size_t count = size_t(1) << n;
    Function f{n, m, std::vector<U>(count, 0), path};
    std::vector<bool> seen(m, false);
    for (; std::getline(input, line);) {
        std::string clean = compact_anf_line(line);
        if (clean.empty()) continue;
        if (clean[0] != 'f' && clean[0] != 'F') throw std::runtime_error("ANF 坐标行须写成 f1 = 表达式。");
        size_t pos = 1;
        while (pos < clean.size() && std::isdigit((unsigned char)clean[pos])) ++pos;
        if (pos == 1 || pos >= clean.size() || clean[pos] != '=') throw std::runtime_error("ANF 坐标行须写成 f1 = 表达式。");
        int index = positive_int(clean.substr(1, pos - 1), "坐标下标");
        if (index > m) throw std::runtime_error("ANF 坐标下标超过 m。");
        if (seen[index - 1]) throw std::runtime_error("ANF 中的 f" + std::to_string(index) + " 重复。");
        seen[index - 1] = true;
        std::string expr = clean.substr(pos + 1);
        const std::string xor_sign = "\xE2\x8A\x95";
        for (size_t at = 0; (at = expr.find(xor_sign, at)) != std::string::npos;) expr.replace(at, xor_sign.size(), "+");
        if (expr.empty()) throw std::runtime_error("ANF 的 f" + std::to_string(index) + " 表达式为空。");
        std::vector<std::uint8_t> coefficients(count, 0);
        size_t start = 0;
        while (start < expr.size()) {
            size_t end = expr.find('+', start);
            if (end == std::string::npos) end = expr.size();
            if (end == start) throw std::runtime_error("ANF 的 f" + std::to_string(index) + " 出现空项或连续加号。");
            std::string term = expr.substr(start, end - start);
            if (term == "1") coefficients[0] ^= 1;
            else if (term != "0") {
                U mask = 0;
                size_t cursor = 0;
                while (cursor < term.size()) {
                    if (term[cursor] != 'x' && term[cursor] != 'X') throw std::runtime_error("ANF 单项式语法错误：" + term);
                    size_t digit_start = ++cursor;
                    while (cursor < term.size() && std::isdigit((unsigned char)term[cursor])) ++cursor;
                    if (cursor == digit_start) throw std::runtime_error("ANF 变量缺少下标：" + term);
                    int variable = positive_int(term.substr(digit_start, cursor - digit_start), "变量下标");
                    if (variable > n) throw std::runtime_error("ANF 变量下标超过 n。");
                    mask |= U(1) << (n - variable);
                    if (cursor < term.size() && term[cursor] == '*') {
                        ++cursor;
                        if (cursor == term.size()) throw std::runtime_error("ANF 单项式末尾不能是乘号。");
                    }
                }
                coefficients[mask] ^= 1;
            }
            if (end == expr.size()) break;
            start = end + 1;
            if (start == expr.size()) throw std::runtime_error("ANF 表达式不能以加号结尾。");
        }
        auto coordinate = mobius(std::move(coefficients), n);
        for (size_t x = 0; x < count; ++x) f.truth[x] |= U(coordinate[x]) << (m - index);
    }
    for (int i = 0; i < m; ++i) if (!seen[i]) throw std::runtime_error("ANF 缺少坐标函数 f" + std::to_string(i + 1) + "。");
    return f;
}
static Function load_function(const fs::path& path, int n_arg, int m_arg, const std::string& radix) {
    if (radix != "bin" && radix != "hex") throw std::runtime_error("输入进制必须为 bin 或 hex。");
    auto tokens = tokenize(read_file(path));
    if (tokens.empty()) throw std::runtime_error("输入文件为空。");
    int width = radix == "hex" ? (m_arg + 3) / 4 : m_arg;
    if (m_arg == 0) {
        std::string first = tokens.front();
        if (first.rfind("0x", 0) == 0 || first.rfind("0X", 0) == 0) first.erase(0, 2);
        width = int(first.size());
        m_arg = radix == "hex" ? 4 * width : width;
    }
    if (m_arg < 1 || m_arg > 8) throw std::runtime_error("输出位数 m 须在 1 到 8 之间。");
    width = radix == "hex" ? (m_arg + 3) / 4 : m_arg;
    if (tokens.size() == 1) {
        std::string only = tokens.front();
        if (only.rfind("0x", 0) == 0 || only.rfind("0X", 0) == 0) only.erase(0, 2);
        if (only.size() > size_t(width) && only.size() % size_t(width) == 0) {
            tokens.clear();
            for (size_t pos = 0; pos < only.size(); pos += width) tokens.push_back(only.substr(pos, width));
        }
    }
    if (tokens.size() < 2 || !std::has_single_bit(tokens.size())) throw std::runtime_error("输出词数须为 2^n，且至少为 2。");
    int n = int(std::bit_width(tokens.size()) - 1);
    if (n_arg && n_arg != n) throw std::runtime_error("输入词数与指定 n 不一致。");
    if (n > 12) throw std::runtime_error("当前精确分析支持 n≤12。");
    if (m_arg > n) throw std::runtime_error("当前版本要求 m≤n。");
    Function f{n, m_arg, {}, path};
    f.truth.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::string token = tokens[i];
        if (radix == "hex" && (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0)) token.erase(0, 2);
        if (token.size() != size_t(width)) throw std::runtime_error("第 " + std::to_string(i + 1) + " 个输出词宽度不等于指定 m。");
        U value = 0;
        for (char c : token) {
            int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (digit < 0 || digit >= (radix == "bin" ? 2 : 16)) throw std::runtime_error("第 " + std::to_string(i + 1) + " 个输出词含非法字符。");
            value = value * (radix == "bin" ? 2 : 16) + U(digit);
        }
        if (value >= (U(1) << m_arg)) throw std::runtime_error("输出词的值超过 m 位范围。");
        f.truth.push_back(value);
    }
    return f;
}
static Function load_transposed_function(const fs::path& path, int n, int m, const std::string& radix) {
    if (n < 1 || n > 12 || m < 1 || m > std::min(n, 8))
        throw std::runtime_error("转置输入要求 1≤n≤12、1≤m≤min(n,8)。");
    if (radix != "bin" && radix != "hex") throw std::runtime_error("输入进制必须为 bin 或 hex。");
    const size_t count = size_t(1) << n;
    std::istringstream input(read_file(path));
    std::vector<std::string> rows;
    for (std::string line; std::getline(input, line);) {
        std::string compact;
        for (char c : line) {
            if (c == ' ' || c == '\t' || c == '\r' || c == ',' || c == ';' || c == '|') continue;
            compact += c;
        }
        if (compact.empty()) continue;
        if (compact.size() >= 2 && compact[0] == '0' &&
            ((radix == "bin" && (compact[1] == 'b' || compact[1] == 'B')) ||
             (radix == "hex" && (compact[1] == 'x' || compact[1] == 'X')))) compact.erase(0, 2);
        rows.push_back(std::move(compact));
    }
    if (rows.size() != size_t(m))
        throw std::runtime_error("转置真值表须有恰好 m=" + std::to_string(m) + " 个非空坐标行，实际为 " + std::to_string(rows.size()) + " 行。");
    Function f{n, m, std::vector<U>(count, 0), path};
    for (size_t row = 0; row < rows.size(); ++row) {
        std::string bits;
        if (radix == "bin") {
            if (rows[row].size() != count)
                throw std::runtime_error("转置真值表第 " + std::to_string(row + 1) + " 行须有 " + std::to_string(count) + " 个二进制位。");
            bits = rows[row];
            if (bits.find_first_not_of("01") != std::string::npos)
                throw std::runtime_error("转置真值表第 " + std::to_string(row + 1) + " 行含非法二进制字符。");
        } else {
            const size_t digits = (count + 3) / 4;
            if (rows[row].size() != digits)
                throw std::runtime_error("转置真值表第 " + std::to_string(row + 1) + " 行须有 " + std::to_string(digits) + " 个十六进制位。");
            for (char c : rows[row]) {
                int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
                if (digit < 0) throw std::runtime_error("转置真值表第 " + std::to_string(row + 1) + " 行含非法十六进制字符。");
                bits += bits_fixed(U(digit), 4);
            }
            const size_t padding = digits * 4 - count;
            if (bits.find('1') < padding)
                throw std::runtime_error("转置真值表第 " + std::to_string(row + 1) + " 行的高位填充必须为 0。");
            bits.erase(0, padding);
        }
        for (size_t x = 0; x < count; ++x) f.truth[x] |= U(bits[x] - '0') << (m - 1 - row);
    }
    return f;
}
static int parity(U v) { return std::popcount(v) & 1; }
static std::vector<std::uint8_t> component(const Function& f, U b) {
    std::vector<std::uint8_t> out(f.truth.size());
    for (size_t x = 0; x < out.size(); ++x) out[x] = std::uint8_t(parity(b & f.truth[x]));
    return out;
}
static std::vector<std::uint8_t> mobius(std::vector<std::uint8_t> a, int n) {
    const size_t N = a.size();
    for (int bit = 0; bit < n; ++bit) for (size_t mask = 0; mask < N; ++mask) if (mask & (size_t(1) << bit)) a[mask] ^= a[mask ^ (size_t(1) << bit)];
    return a;
}
static std::vector<std::int32_t> fwht(std::vector<std::int32_t> a) {
    for (size_t len = 1; len < a.size(); len <<= 1) for (size_t i = 0; i < a.size(); i += 2 * len)
        for (size_t j = 0; j < len; ++j) { auto u = a[i+j], v = a[i+j+len]; a[i+j] = u+v; a[i+j+len] = u-v; }
    return a;
}
struct Session {
    const Function& f;
    std::unordered_map<U, std::vector<std::int32_t>> walsh_cache;
    std::unordered_map<U, std::vector<std::uint8_t>> anf_cache;
    explicit Session(const Function& source) : f(source) {}
    const std::vector<std::int32_t>& walsh(U b) {
        auto it = walsh_cache.find(b);
        if (it != walsh_cache.end()) return it->second;
        std::vector<std::int32_t> signs(f.truth.size());
        for (size_t x = 0; x < signs.size(); ++x) signs[x] = parity(b & f.truth[x]) ? -1 : 1;
        return walsh_cache.emplace(b, fwht(std::move(signs))).first->second;
    }
    const std::vector<std::uint8_t>& anf(U b) {
        auto it = anf_cache.find(b);
        if (it != anf_cache.end()) return it->second;
        return anf_cache.emplace(b, mobius(component(f, b), f.n)).first->second;
    }
};
static int degree(const std::vector<std::uint8_t>& anf) {
    int result = -1;
    for (size_t mask = 0; mask < anf.size(); ++mask) if (anf[mask]) result = std::max(result, int(std::popcount(U(mask))));
    return result;
}
static std::string anf_text(const std::vector<std::uint8_t>& anf, int n) {
    std::vector<std::string> terms;
    for (size_t mask = 0; mask < anf.size(); ++mask) if (anf[mask]) {
        if (!mask) { terms.push_back("1"); continue; }
        std::string term;
        for (int i = 1; i <= n; ++i) if (mask & (size_t(1) << (n-i))) {
            if (!term.empty()) term += '*';
            term += "x" + std::to_string(i);
        }
        terms.push_back(term);
    }
    if (terms.empty()) return "0";
    std::string out;
    for (size_t i = 0; i < terms.size(); ++i) { if (i) out += " + "; out += terms[i]; }
    return out;
}
static std::vector<Count> output_histogram(const Function& f) {
    std::vector<Count> counts(size_t(1) << f.m, 0);
    for (U value : f.truth) ++counts[value];
    return counts;
}
static std::vector<Count> ddt_row(const Function& f, U a) {
    std::vector<Count> row(size_t(1) << f.m, 0);
    for (U x = 0; x < f.truth.size(); ++x) ++row[f.truth[x] ^ f.truth[x ^ a]];
    return row;
}
static bool has_annihilator(const std::vector<std::uint8_t>& truth, int d, bool complement) {
    std::vector<U> monos;
    for (U mask = 0; mask < truth.size(); ++mask) if (std::popcount(mask) <= d) monos.push_back(mask);
    const size_t columns = monos.size(), words = (columns + 63) / 64;
    std::vector<std::vector<std::uint64_t>> basis(columns);
    size_t rank = 0;
    for (U x = 0; x < truth.size(); ++x) {
        if ((truth[x] ^ std::uint8_t(complement)) == 0) continue;
        std::vector<std::uint64_t> row(words, 0);
        for (size_t j = 0; j < columns; ++j) if ((x & monos[j]) == monos[j]) row[j / 64] |= std::uint64_t(1) << (j % 64);
        for (size_t j = 0; j < columns; ++j) if (row[j / 64] & (std::uint64_t(1) << (j % 64))) {
            if (basis[j].empty()) { basis[j] = std::move(row); ++rank; break; }
            for (size_t k = j / 64; k < words; ++k) row[k] ^= basis[j][k];
        }
        if (rank == columns) return false;
    }
    return rank < columns;
}
static int component_ai(const std::vector<std::uint8_t>& truth, int n) {
    for (int d = 0; d <= (n + 1) / 2; ++d) if (has_annihilator(truth, d, false) || has_annihilator(truth, d, true)) return d;
    return -1;
}
struct Result { std::string id, title, value, detail; };
static Result result(std::string id, std::string title, std::string value, std::string detail = {}) { return {std::move(id), std::move(title), std::move(value), std::move(detail)}; }
static std::string result_json(const Result& r) {
    return "{\"id\":" + json_string(r.id) + ",\"title\":" + json_string(r.title) + ",\"value\":" + json_string(r.value) + ",\"detail\":" + json_string(r.detail) + "}";
}
static std::string decimal(long double v, int places = 6) {
    std::ostringstream s; s << std::fixed << std::setprecision(places) << v; return s.str();
}
static std::set<std::string> parse_metrics(const std::string& csv) {
    const std::set<std::string> valid = {"balance","max_degree","min_degree","ai","nonlinearity","resiliency","bent","almost_optimal","diff_uniformity","diff_probability","diff_deviation","diff_stddev","pn","apn","linearity","linear_bias","linear_probability","ab"};
    std::set<std::string> out; std::istringstream stream(csv);
    for (std::string id; std::getline(stream, id, ',');) {
        if (id.empty()) continue;
        if (!valid.contains(id)) throw std::runtime_error("未知指标：" + id);
        out.insert(id);
    }
    return out;
}
static bool wants(const std::set<std::string>& metrics, const std::string& id) { return metrics.contains(id); }
struct Summary {
    std::optional<int> nonlinearity;
    std::optional<U> weak_b, weak_a;
    std::optional<int> linear_max;
    std::optional<U> linear_a, linear_b;
    std::optional<Count> delta, max_deviation;
    std::optional<U> diff_a, diff_b;
    std::optional<long double> stddev;
};
static std::string csv_table(const Function& f, bool differential, Session& session) {
    const U N = U(f.truth.size()), M = U(1) << f.m;
    std::ostringstream out;
    out << (differential ? "input_difference/output_difference" : "input_mask/output_mask");
    for (U b = 0; b < M; ++b) out << ',' << hex_fixed(b, f.m);
    out << '\n';
    std::vector<std::vector<std::int32_t>> lat;
    if (!differential) { lat.reserve(M); for (U b = 0; b < M; ++b) lat.push_back(session.walsh(b)); }
    for (U a = 0; a < N; ++a) {
        out << hex_fixed(a, f.n);
        if (differential) { auto row = ddt_row(f, a); for (Count v : row) out << ',' << v; }
        else for (U b = 0; b < M; ++b) out << ',' << lat[b][a];
        out << '\n';
    }
    return out.str();
}
static fs::path new_output_dir(const fs::path& desired) {
    if (!fs::exists(desired)) { fs::create_directories(desired); return desired; }
    if (!fs::is_directory(desired)) throw std::runtime_error("输出路径不是文件夹。");
    if (fs::directory_iterator(desired) == fs::directory_iterator()) return desired;
    for (int i = 2; i < 10000; ++i) {
        fs::path candidate = desired / (L"run-" + std::to_wstring(i));
        if (!fs::exists(candidate)) { fs::create_directories(candidate); return candidate; }
    }
    throw std::runtime_error("无法创建新的运行目录。");
}
static std::string paths_json(const std::vector<fs::path>& paths) {
    std::string out = "[";
    for (size_t i = 0; i < paths.size(); ++i) { if (i) out += ','; out += json_string(utf8(paths[i].wstring())); }
    return out + "]";
}
static std::vector<fs::path> export_results(const Function& f, Session& session, const fs::path& base, bool want_ddt, bool want_lat) {
    fs::path dir = new_output_dir(base);
    std::vector<fs::path> paths;
    auto add = [&](const wchar_t* name, const std::string& content) { fs::path p = dir / name; write_file(p, content); paths.push_back(p); };
    std::string truth = "n=" + std::to_string(f.n) + "; m=" + std::to_string(f.m) + "\n";
    for (U y : f.truth) truth += bits_fixed(y, f.m) + "\n";
    add(L"Truth_table.txt", truth);
    std::string anf = "n=" + std::to_string(f.n) + "; m=" + std::to_string(f.m) + "\n";
    for (int i = 1; i <= f.m; ++i) anf += "f" + std::to_string(i) + " = " + anf_text(session.anf(U(1) << (f.m-i)), f.n) + "\n";
    add(L"ANF.txt", anf);
    if (want_ddt) add(L"DDT.csv", csv_table(f, true, session));
    if (want_lat) add(L"LAT.csv", csv_table(f, false, session));
    return paths;
}
int wmain(int argc, wchar_t** argv) {
    try {
        fs::path input, output;
        int n_arg = 0, m_arg = 0;
        std::string radix = "hex", kind = "auto", metrics_csv;
        bool want_ddt = false, want_lat = false, transpose = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring key = argv[i];
            if (key == L"--ddt") { want_ddt = true; continue; }
            if (key == L"--lat") { want_lat = true; continue; }
            if (key == L"--transpose") { transpose = true; continue; }
            if (++i >= argc) throw std::runtime_error("命令行参数缺少值。");
            std::wstring value = argv[i];
            if (key == L"--input") input = value;
            else if (key == L"--out") output = value;
            else if (key == L"--n") n_arg = positive_int(utf8(value), "n");
            else if (key == L"--m") m_arg = positive_int(utf8(value), "m");
            else if (key == L"--radix") radix = utf8(value);
            else if (key == L"--kind") kind = utf8(value);
            else if (key == L"--metrics") metrics_csv = utf8(value);
            else throw std::runtime_error("未知命令行参数：" + utf8(key));
        }
        if (input.empty()) throw std::runtime_error("缺少输入 TXT 文件。");
        if (kind != "auto" && kind != "truth" && kind != "anf") throw std::runtime_error("输入类型无效。");
        auto metrics = parse_metrics(metrics_csv);
        if (kind == "auto") kind = looks_like_anf(input) ? "anf" : "truth";
        if (kind == "truth" && looks_like_anf(input)) throw std::runtime_error("文件含 ANF 坐标函数，不能按真值表读取。");
        const bool effective_transpose = transpose && kind != "anf";
        Function f = kind == "anf" ? load_anf_function(input, n_arg, m_arg) :
            effective_transpose ? load_transposed_function(input, n_arg, m_arg, radix) : load_function(input, n_arg, m_arg, radix);
        Session session(f);
        const U N = U(f.truth.size()), M = U(1) << f.m;
        const Count expected = N / M;
        std::vector<Result> results;
        Summary summary;
        auto add = [&](const std::string& id, const std::string& title, const std::string& value, const std::string& detail = "") { if (wants(metrics,id)) results.push_back(result(id,title,value,detail)); };
        if (wants(metrics,"balance")) {
            auto hist = output_histogram(f);
            bool balanced = std::all_of(hist.begin(), hist.end(), [&](Count v) { return v == expected; });
            add("balance", "平衡性", balanced ? "满足" : "不满足", "每个输出值应出现 " + std::to_string(expected) + " 次；置换=" + (f.permutation() ? "是" : "否"));
        }
        if (wants(metrics,"max_degree") || wants(metrics,"min_degree")) {
            int max_d = -2, min_d = f.n + 1; U max_b = 0, min_b = 0;
            if (wants(metrics,"max_degree")) for (int i = 0; i < f.m; ++i) { U b = U(1) << i; int d = degree(session.anf(b)); if (d > max_d) { max_d = d; max_b = b; } }
            if (wants(metrics,"min_degree")) for (U b = 1; b < M; ++b) { int d = degree(session.anf(b)); if (d < min_d) { min_d = d; min_b = b; } }
            add("max_degree", "最大代数次数", max_d < 0 ? "零多项式" : std::to_string(max_d), "分量掩码 " + hex_fixed(max_b, f.m));
            add("min_degree", "最小代数次数", min_d < 0 ? "零多项式" : std::to_string(min_d), "分量掩码 " + hex_fixed(min_b, f.m));
        }
        if (wants(metrics,"ai")) {
            if (f.n > 8 || f.m > 6) add("ai", "代数免疫阶 AI", "超出范围", "精确求解上限 n≤8、m≤6");
            else { int best = f.n + 1; U best_b = 0; for (U b = 1; b < M; ++b) { int ai = component_ai(component(f,b),f.n); if (ai < best) { best = ai; best_b = b; } if (best == 0) break; } add("ai", "代数免疫阶 AI", std::to_string(best), "最弱分量掩码 " + hex_fixed(best_b,f.m)); }
        }
        bool need_nl = wants(metrics,"nonlinearity") || wants(metrics,"bent") || wants(metrics,"almost_optimal") || wants(metrics,"ab");
        bool need_res = wants(metrics,"resiliency");
        bool need_linear = wants(metrics,"linearity") || wants(metrics,"linear_bias") || wants(metrics,"linear_probability");
        if (need_nl || need_res || need_linear) {
            int peak = 0, linear_peak = -1, first_nonzero_weight = f.n + 1;
            U peak_a = 0, peak_b = 0, linear_a = 1, linear_b = 1;
            for (U b = 1; b < M; ++b) {
                const auto& w = session.walsh(b);
                for (U a = 0; a < N; ++a) {
                    int absolute = std::abs(w[a]);
                    if (need_nl && absolute > peak) { peak = absolute; peak_a = a; peak_b = b; }
                    if (need_res && w[a] && std::popcount(a) < first_nonzero_weight) first_nonzero_weight = std::popcount(a);
                    if (need_linear && a && absolute > linear_peak) { linear_peak = absolute; linear_a = a; linear_b = b; }
                }
            }
            if (need_nl) {
                summary.nonlinearity = int(N)/2 - peak/2; summary.weak_a = peak_a; summary.weak_b = peak_b;
                add("nonlinearity", "向量非线性度", std::to_string(*summary.nonlinearity), "最弱分量 " + hex_fixed(peak_b,f.m) + "，输入掩码 " + hex_fixed(peak_a,f.n));
                if (wants(metrics,"bent")) { bool yes = f.n % 2 == 0 && f.m <= f.n/2 && peak == (1 << (f.n/2)); add("bent","多输出 Bent",yes?"满足":"不满足","全部非零分量需具有平坦 Walsh 谱"); }
                if (wants(metrics,"almost_optimal")) { int threshold = int(N)/2 - (f.n % 2 ? (1 << ((f.n-1)/2)) : (1 << (f.n/2))); add("almost_optimal","几乎最优非线性度",*summary.nonlinearity >= threshold ? "满足" : "不满足","门槛 " + std::to_string(threshold) + "；严格几乎最优需大于门槛"); }
            }
            if (need_res) { int order = first_nonzero_weight == 0 ? -1 : std::min(f.n-f.m, first_nonzero_weight-1); add("resiliency","弹性阶",order<0?"不具备0阶弹性":std::to_string(order),"检查全部非零输出分量的低重量 Walsh 系数"); }
            if (need_linear) {
                summary.linear_max = linear_peak; summary.linear_a = linear_a; summary.linear_b = linear_b;
                std::string witness = "输入掩码 " + hex_fixed(linear_a,f.n) + "，输出掩码 " + hex_fixed(linear_b,f.m) + "，带符号系数 " + std::to_string(session.walsh(linear_b)[linear_a]);
                add("linearity","最大线性相关幅度",std::to_string(linear_peak),witness);
                add("linear_bias","最大绝对线性偏差",decimal((long double)linear_peak/(2*N)),witness);
                add("linear_probability","最佳线性近似概率",decimal(0.5L+(long double)linear_peak/(2*N)),"等式或互补式的最大命中概率；"+witness);
            }
        }
        bool need_diff = wants(metrics,"diff_uniformity") || wants(metrics,"diff_probability") || wants(metrics,"diff_deviation") || wants(metrics,"diff_stddev") || wants(metrics,"pn") || wants(metrics,"apn") || wants(metrics,"ab");
        if (need_diff) {
            Count maximum = 0, deviation = 0, squared = 0; U max_a = 0, max_b = 0;
            bool need_max = wants(metrics,"diff_uniformity") || wants(metrics,"diff_probability") || wants(metrics,"pn") || wants(metrics,"apn") || wants(metrics,"ab");
            bool need_dev = wants(metrics,"diff_deviation"); bool need_std = wants(metrics,"diff_stddev");
            for (U a = 1; a < N; ++a) {
                auto row = ddt_row(f,a);
                for (U b = 0; b < M; ++b) {
                    Count value = row[b];
                    if (need_max && value > maximum) { maximum = value; max_a = a; max_b = b; }
                    if (need_dev || need_std) { Count diff = value > expected ? value-expected : expected-value; if (need_dev) deviation = std::max(deviation,diff); if (need_std) squared += diff*diff; }
                }
            }
            if (need_max) { summary.delta = maximum; summary.diff_a = max_a; summary.diff_b = max_b; }
            if (need_dev) summary.max_deviation = deviation;
            if (need_std) summary.stddev = std::sqrt((long double)squared/(M*(N-1)));
            std::string witness = "输入差分 " + hex_fixed(max_a,f.n) + "，输出差分 " + hex_fixed(max_b,f.m);
            add("diff_uniformity","差分均匀度",std::to_string(maximum),witness);
            add("diff_probability","最大差分概率",decimal((long double)maximum/N),witness);
            add("diff_deviation","差分最大偏差",std::to_string(deviation),"相对理想均值 " + std::to_string(expected));
            add("diff_stddev","差分标准差",decimal(summary.stddev.value_or(0)),"按所有非零输入差分与全部输出差分计算");
            add("pn","PN 判定",maximum == expected ? "满足" : "不满足","要求每个非零输入差分的输出分布均匀");
            add("apn","APN 判定",f.n == f.m && maximum == 2 ? "满足" : "不满足","要求 n=m 且差分均匀度为 2");
        }
        if (wants(metrics,"ab")) {
            bool yes = f.n == f.m && f.n >= 3 && (f.n & 1) && summary.delta == 2 && summary.nonlinearity == int(N)/2-(1<<((f.n-1)/2));
            add("ab","AB 判定",yes?"满足":"不满足","要求奇数维方阵、APN 且达到指定非线性度");
        }
        if (output.empty()) output = input.parent_path() / L"vbf_output";
        if ((want_ddt || want_lat) && std::uint64_t(N)*M > 131072) throw std::runtime_error("完整 DDT/LAT 超过 131072 个单元上限；可只选摘要指标。");
        auto files = export_results(f,session,output,want_ddt,want_lat);
        std::string joined = "["; for (size_t i = 0; i < results.size(); ++i) { if (i) joined += ','; joined += result_json(results[i]); } joined += ']';
        std::cout << "{\"ok\":true,\"input\":{\"name\":" << json_string(utf8(input.filename().wstring())) << ",\"n\":" << f.n << ",\"m\":" << f.m << ",\"kind\":" << json_string(kind) << ",\"transposed\":" << (effective_transpose?"true":"false") << ",\"permutation\":" << (f.permutation()?"true":"false") << "},\"files\":" << paths_json(files) << ",\"results\":" << joined << "}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "{\"ok\":false,\"error\":" << json_string(e.what()) << "}\n";
        return 1;
    }
}
