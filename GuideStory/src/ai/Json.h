#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 최소 JSON 리더/라이터 — AI 채널 본문 전용.
//
// [왜 자체 구현인가]
//   외부 라이브러리(nlohmann 등)를 vcpkg 에 추가하면 엔진 빌드 의존성이 늘어난다. 지금
//   필요한 것은 평탄한 객체 하나와 정수 배열 하나를 읽는 일이고, 그 범위에서 직접 쓰는 편이
//   의존성 추가보다 싸다. (서버의 PBKDF2/SHA-256 을 직접 구현한 것과 같은 판단이다.)
//
// [그래서 지키는 것]
//   응답은 **프로세스 밖에서 온다.** 파서는 어떤 입력에도 크래시하지 않고 false 를 돌려야 한다.
//   - 재귀 깊이 제한(스택 폭발 방지)
//   - 모든 위치에서 입력 끝 검사
//   - \uXXXX 는 서로게이트 쌍까지 처리해 UTF-8 로 변환(파이썬 json.dumps 기본값이 비ASCII를
//     \u 로 이스케이프한다 — 서버 오류 메시지가 한국어다)
//
// [지원하지 않는 것]  주석, 후행 쉼표, NaN/Infinity, 큰 정수의 무손실 보존(double 로 읽는다).
// 우리 프로토콜에 필요 없고, 넓힐수록 검증할 표면만 늘어난다.
namespace gs::ai::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type        type = Type::Null;
    bool        boolean = false;
    double      number = 0.0;
    std::string text;
    std::vector<Value>                        items;   // Array
    std::vector<std::pair<std::string, Value>> fields; // Object

    bool IsNull()   const { return type == Type::Null; }
    bool IsBool()   const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsArray()  const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    // 객체 멤버 찾기. 없으면 nullptr — 호출측이 기본값을 정한다.
    const Value* Find(std::string_view key) const {
        if (type != Type::Object) return nullptr;
        for (const auto& kv : fields)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }

    // 타입이 다르면 조용히 기본값을 돌려준다. 응답이 어긋났을 때 예외로 편집을 끊지 않는다.
    bool GetBool(std::string_view key, bool fallback = false) const {
        const Value* v = Find(key);
        return (v && v->IsBool()) ? v->boolean : fallback;
    }
    double GetNumber(std::string_view key, double fallback = 0.0) const {
        const Value* v = Find(key);
        return (v && v->IsNumber()) ? v->number : fallback;
    }
    int GetInt(std::string_view key, int fallback = 0) const {
        return static_cast<int>(GetNumber(key, static_cast<double>(fallback)));
    }
    std::string GetString(std::string_view key, std::string fallback = {}) const {
        const Value* v = Find(key);
        return (v && v->IsString()) ? v->text : std::move(fallback);
    }
};

namespace detail {

inline constexpr int kMaxDepth = 32;

struct Parser {
    std::string_view s;
    std::size_t      i = 0;
    std::string      error;

    bool Fail(const char* why) {
        if (error.empty()) error = why;
        return false;
    }
    void SkipWs() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }
    bool Eof() const { return i >= s.size(); }

    // 코드포인트를 UTF-8 로 덧붙인다.
    static void AppendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool ParseHex4(uint32_t& out) {
        if (i + 4 > s.size()) return Fail("\\u 뒤 자릿수 부족");
        out = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s[i++];
            out <<= 4;
            if (c >= '0' && c <= '9')      out |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
            else return Fail("\\u 뒤 16진수가 아님");
        }
        return true;
    }

    bool ParseString(std::string& out) {
        if (Eof() || s[i] != '"') return Fail("문자열 시작이 아님");
        ++i;
        out.clear();
        while (true) {
            if (Eof()) return Fail("문자열이 닫히지 않음");
            const char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (Eof()) return Fail("이스케이프가 잘림");
            const char e = s[i++];
            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!ParseHex4(cp)) return false;
                    // 서로게이트 쌍: 상위(D800~DBFF) 뒤에 하위(DC00~DFFF)가 와야 한다.
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                            i += 2;
                            uint32_t lo = 0;
                            if (!ParseHex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF)
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            else
                                cp = 0xFFFD; // 짝이 안 맞으면 대체 문자로 — 실패시키진 않는다
                        } else {
                            cp = 0xFFFD;
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        cp = 0xFFFD; // 짝 없는 하위 서로게이트
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return Fail("알 수 없는 이스케이프");
            }
        }
    }

    bool ParseNumber(double& out) {
        const std::size_t start = i;
        if (!Eof() && (s[i] == '-' || s[i] == '+')) ++i;
        bool anyDigit = false;
        while (!Eof() && s[i] >= '0' && s[i] <= '9') { ++i; anyDigit = true; }
        if (!Eof() && s[i] == '.') {
            ++i;
            while (!Eof() && s[i] >= '0' && s[i] <= '9') { ++i; anyDigit = true; }
        }
        if (!anyDigit) return Fail("숫자가 아님");
        if (!Eof() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (!Eof() && (s[i] == '-' || s[i] == '+')) ++i;
            bool expDigit = false;
            while (!Eof() && s[i] >= '0' && s[i] <= '9') { ++i; expDigit = true; }
            if (!expDigit) return Fail("지수부가 비었음");
        }
        try {
            out = std::stod(std::string(s.substr(start, i - start)));
        } catch (...) {
            return Fail("숫자 변환 실패");
        }
        return true;
    }

    bool Literal(std::string_view lit) {
        if (s.compare(i, lit.size(), lit) != 0) return false;
        i += lit.size();
        return true;
    }

    bool ParseValue(Value& out, int depth) {
        if (depth > kMaxDepth) return Fail("중첩이 너무 깊음");
        SkipWs();
        if (Eof()) return Fail("입력이 갑자기 끝남");

        const char c = s[i];
        if (c == '"') {
            out.type = Value::Type::String;
            return ParseString(out.text);
        }
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == 't') { if (!Literal("true"))  return Fail("true 아님");
                        out.type = Value::Type::Bool; out.boolean = true;  return true; }
        if (c == 'f') { if (!Literal("false")) return Fail("false 아님");
                        out.type = Value::Type::Bool; out.boolean = false; return true; }
        if (c == 'n') { if (!Literal("null"))  return Fail("null 아님");
                        out.type = Value::Type::Null; return true; }

        out.type = Value::Type::Number;
        return ParseNumber(out.number);
    }

    bool ParseArray(Value& out, int depth) {
        ++i; // '['
        out.type = Value::Type::Array;
        SkipWs();
        if (!Eof() && s[i] == ']') { ++i; return true; }
        while (true) {
            Value v;
            if (!ParseValue(v, depth + 1)) return false;
            out.items.push_back(std::move(v));
            SkipWs();
            if (Eof()) return Fail("배열이 닫히지 않음");
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == ']') { ++i; return true; }
            return Fail("배열에서 , 또는 ] 를 기대");
        }
    }

    bool ParseObject(Value& out, int depth) {
        ++i; // '{'
        out.type = Value::Type::Object;
        SkipWs();
        if (!Eof() && s[i] == '}') { ++i; return true; }
        while (true) {
            SkipWs();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (Eof() || s[i] != ':') return Fail("키 뒤에 : 가 없음");
            ++i;
            Value v;
            if (!ParseValue(v, depth + 1)) return false;
            out.fields.emplace_back(std::move(key), std::move(v));
            SkipWs();
            if (Eof()) return Fail("객체가 닫히지 않음");
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == '}') { ++i; return true; }
            return Fail("객체에서 , 또는 } 를 기대");
        }
    }
};

} // namespace detail

// 전체 입력을 하나의 값으로 파싱한다. 실패하면 false 와 사유.
inline bool Parse(std::string_view text, Value& out, std::string& error) {
    detail::Parser p{text};
    out = Value{};
    if (!p.ParseValue(out, 0)) {
        error = p.error.empty() ? "JSON 파싱 실패" : p.error;
        return false;
    }
    p.SkipWs();
    if (!p.Eof()) {
        error = "JSON 뒤에 잉여 데이터";
        return false;
    }
    return true;
}

// --- 쓰기 (요청 조립용 — 필요한 만큼만) --------------------------------------

// 문자열을 JSON 리터럴로 이스케이프한다. 제어문자는 \u00XX 로.
inline std::string Escape(std::string_view v) {
    std::string out;
    out.reserve(v.size() + 2);
    out.push_back('"');
    for (const char ch : v) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    static const char* kHex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4) & 0xF]);
                    out.push_back(kHex[c & 0xF]);
                } else {
                    out.push_back(ch); // UTF-8 바이트는 그대로 (유효한 JSON이다)
                }
        }
    }
    out.push_back('"');
    return out;
}

} // namespace gs::ai::json
