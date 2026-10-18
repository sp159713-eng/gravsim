#pragma once
#include <string>
#include <vector>

struct Json {
    enum Kind { NUL, NUM, STR, ARR, OBJ, BOOL };
    Kind k = NUL;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::string> keys;
    std::vector<Json> vals;

    static Json number(double v) { Json j; j.k = NUM; j.n = v; return j; }
    static Json boolean(bool v) { Json j; j.k = BOOL; j.n = v ? 1 : 0; return j; }
    static Json str(const std::string& v) { Json j; j.k = STR; j.s = v; return j; }
    static Json array() { Json j; j.k = ARR; return j; }
    static Json object() { Json j; j.k = OBJ; return j; }
    void push(const Json& v) { a.push_back(v); }
    void set(const std::string& key, const Json& v) { keys.push_back(key); vals.push_back(v); }
    const Json* get(const std::string& key) const {
        for (size_t i = 0; i < keys.size(); i++)
            if (keys[i] == key) return &vals[i];
        return nullptr;
    }
    double num(const std::string& key, double def) const {
        const Json* j = get(key);
        return j && (j->k == NUM || j->k == BOOL) ? j->n : def;
    }
};

std::string jsonDump(const Json& j);
bool jsonParse(const std::string& text, Json& out);
