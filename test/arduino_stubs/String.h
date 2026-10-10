// Minimal Arduino String stub for native unit tests (subset used by this project).
#ifndef TEST_STRING_H
#define TEST_STRING_H

#include <string>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>

class String {
public:
    String() {}
    String(const char* s) : s_(s ? s : "") {}
    String(const String&) = default;
    String(String&&) = default;
    String(const std::string& s) : s_(s) {}
    String(double v, int decimals = 2) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        s_ = buf;
    }

    String& operator=(const String&) = default;
    String& operator=(const char* s) { s_ = (s ? s : ""); return *this; }

    unsigned int length() const { return (unsigned int)s_.size(); }
    const char* c_str() const { return s_.c_str(); }
    char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
    char operator[](unsigned int i) const { return charAt(i); }
    bool isEmpty() const { return s_.empty(); }
    bool equals(const String& o) const { return s_ == o.s_; }
    bool equalsIgnoreCase(const String& o) const {
        if (s_.size() != o.s_.size()) return false;
        for (size_t i = 0; i < s_.size(); ++i)
            if (tolower((unsigned char)s_[i]) != tolower((unsigned char)o.s_[i])) return false;
        return true;
    }
    int compareTo(const String& o) const { return s_.compare(o.s_); }

    int indexOf(const char* needle) const {
        std::string::size_type p = s_.find(needle ? needle : "");
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(char c) const {
        std::string::size_type p = s_.find(c);
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(const String& o) const { return indexOf(o.c_str()); }

    bool startsWith(const String& p) const { return s_.rfind(p.s_, 0) == 0; }
    bool endsWith(const String& p) const {
        size_t n = p.s_.size();
        return s_.size() >= n && s_.compare(s_.size() - n, n, p.s_) == 0;
    }

    String substring(unsigned int from) const {
        return from >= s_.size() ? String() : String(s_.substr(from));
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from >= s_.size() || to <= from) return String();
        return String(s_.substr(from, to - from));
    }

    void replace(const String& from, const String& to) {
        if (from.isEmpty()) return;
        std::string out;
        size_t pos = 0, prev = 0;
        while ((pos = s_.find(from.s_, prev)) != std::string::npos) {
            out.append(s_, prev, pos - prev);
            out += to.s_;
            prev = pos + from.s_.size();
        }
        out.append(s_, prev, std::string::npos);
        s_ = out;
    }
    void trim() {
        size_t b = s_.find_first_not_of(" \t\r\n");
        size_t e = s_.find_last_not_of(" \t\r\n");
        s_ = (b == std::string::npos) ? "" : s_.substr(b, e - b + 1);
    }

    // Matches Arduino behavior: strtol prefix parse (invalid -> 0).
    long toInt() const { return strtol(c_str(), nullptr, 10); }
    float toFloat() const { return strtof(c_str(), nullptr); }

    unsigned char reserve(unsigned int) { return 1; } // success, never grows
    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o) { s_ += (o ? o : ""); return *this; }
    String& operator+=(char c) { s_ += c; return *this; }
    // Arduino semantics: returns 1 on success, 0 on (out of) memory.
    bool concat(const String& o) { s_ += o.s_; return true; }
    bool concat(const char* o) { s_ += (o ? o : ""); return true; }
    bool concat(char c) { s_ += c; return true; }
    bool concat(int v) { s_ += std::to_string(v); return true; }
    bool concat(unsigned int v) { s_ += std::to_string(v); return true; }
    bool concat(long v) { s_ += std::to_string(v); return true; }
    bool concat(unsigned long v) { s_ += std::to_string(v); return true; }
    bool concat(float v) { s_ += std::to_string(v); return true; }
    bool concat(double v) { s_ += std::to_string(v); return true; }

    String operator+(const String& o) const { return String(s_ + o.s_); }
    String operator+(const char* o) const { return String(s_ + (o ? o : "")); }
    friend String operator+(const char* a, const String& b) { return String((a ? a : "") + b.s_); }

    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator==(const char* o) const { return s_ == (o ? o : ""); }
    bool operator!=(const String& o) const { return s_ != o.s_; }
    bool operator!=(const char* o) const { return s_ != (o ? o : ""); }
    bool operator<(const String& o) const { return s_ < o.s_; }

private:
    std::string s_;
};

#endif
