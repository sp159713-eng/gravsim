#include "story_json.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void dumpStr(std::string& o, const std::string& s) {
    o.push_back('"');
    for (char c : s) {
        if (c == '"' || c == '\\') {
            o.push_back('\\');
            o.push_back(c);
        } else if (c == '\n') {
            o += "\n";
        } else if ((unsigned char)c < 32) {
            o.push_back(' ');
        } else {
            o.push_back(c);
        }
    }
    o.push_back('"');
}

void dump(std::string& o, const Json& j, int depth) {
    std::string pad((size_t)depth * 2, ' ');
    switch (j.k) {
    case Json::NUL: o += "null"; break;
    case Json::BOOL: o += j.n != 0 ? "true" : "false"; break;
    case Json::NUM: {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%.10g", j.n);
        o += buf;
        break;
    }
    case Json::STR: dumpStr(o, j.s); break;
    case Json::ARR:
        o += "[";
        for (size_t i = 0; i < j.a.size(); i++) {
            if (i) o += ", ";
            dump(o, j.a[i], depth + 1);
        }
        o += "]";
        break;
    case Json::OBJ:
        o += "{\n";
        for (size_t i = 0; i < j.keys.size(); i++) {
            o += pad + "  ";
            dumpStr(o, j.keys[i]);
            o += ": ";
            dump(o, j.vals[i], depth + 1);
            o += i + 1 < j.keys.size() ? ",\n" : "\n";
        }
        o += pad + "}";
        break;
    }
}

struct Parser {
    const char* p;
    const char* end;
    bool ok = true;

    void ws() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    }

    bool lit(const char* w) {
        size_t n = std::strlen(w);
        if ((size_t)(end - p) >= n && std::strncmp(p, w, n) == 0) {
            p += n;
            return true;
        }
        return false;
    }

    std::string str() {
        std::string r;
        p++;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p++;
                r.push_back(*p == 'n' ? '\n' : *p);
            } else {
                r.push_back(*p);
            }
            p++;
        }
        if (p >= end) ok = false;
        else p++;
        return r;
    }

    Json value(int depth) {
        ws();
        Json j;
        if (p >= end || depth > 32) {
            ok = false;
            return j;
        }
        if (*p == '{') {
            j = Json::object();
            p++;
            ws();
            if (p < end && *p == '}') {
                p++;
                return j;
            }
            while (ok) {
                ws();
                if (p >= end || *p != '"') {
                    ok = false;
                    break;
                }
                std::string key = str();
                ws();
                if (p >= end || *p != ':') {
                    ok = false;
                    break;
                }
                p++;
                j.set(key, value(depth + 1));
                ws();
                if (p < end && *p == ',') {
                    p++;
                    continue;
                }
                if (p < end && *p == '}') {
                    p++;
                    break;
                }
                ok = false;
            }
        } else if (*p == '[') {
            j = Json::array();
            p++;
            ws();
            if (p < end && *p == ']') {
                p++;
                return j;
            }
            while (ok) {
                j.push(value(depth + 1));
                ws();
                if (p < end && *p == ',') {
                    p++;
                    continue;
                }
                if (p < end && *p == ']') {
                    p++;
                    break;
                }
                ok = false;
            }
        } else if (*p == '"') {
            j = Json::str(str());
        } else if (lit("true")) {
            j = Json::boolean(true);
        } else if (lit("false")) {
            j = Json::boolean(false);
        } else if (lit("null")) {
            j = Json();
        } else {
            char* e = nullptr;
            double v = std::strtod(p, &e);
            if (e == p) ok = false;
            else {
                p = e;
                j = Json::number(v);
            }
        }
        return j;
    }
};

}

std::string jsonDump(const Json& j) {
    std::string o;
    dump(o, j, 0);
    o.push_back('\n');
    return o;
}

bool jsonParse(const std::string& text, Json& out) {
    Parser ps{text.data(), text.data() + text.size()};
    out = ps.value(0);
    return ps.ok && out.k == Json::OBJ;
}
