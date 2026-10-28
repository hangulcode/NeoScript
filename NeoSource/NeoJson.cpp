#include "NeoJson.h"
#include "NeoVMWorker.h"
#include "../ThirdParty/yyjson/yyjson.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

namespace NeoScript { namespace JsonInternal {
namespace {
using Document = std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)>;
using MutableDocument = std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)>;
struct HeldValue {
    CNeoVMWorker& worker;
    VarInfo value;
    explicit HeldValue(CNeoVMWorker& w) : worker(w) {}
    ~HeldValue() { worker.Var_Release(&value); }
};

std::string Quote(const std::string& key) {
    std::string out = "\"";
    const char hex[] = "0123456789abcdef";
    for (unsigned char c : key) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += char(c);
    }
    return out + '"';
}
std::string KeyPath(const std::string& parent, const std::string& key) {
    auto first = [](unsigned char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 128; };
    bool plain = !key.empty() && first(static_cast<unsigned char>(key[0]));
    for (unsigned char c : key) plain = plain && (first(c) || (c >= '0' && c <= '9'));
    return plain ? parent + '.' + key : parent + '[' + Quote(key) + ']';
}
bool ValidString(const char* data, size_t size) {
    // Validate without creating a second copy or depending on the locale.
    for (size_t i = 0; i < size;) {
        unsigned char c = static_cast<unsigned char>(data[i++]);
        if (!c) return false;
        if (c < 128) continue;
        unsigned count, cp, minimum;
        if (c >= 0xc2 && c <= 0xdf) { count = 1; cp = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { count = 2; cp = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { count = 3; cp = c & 7; minimum = 0x10000; }
        else return false;
        if (size - i < count) return false;
        while (count--) { c = static_cast<unsigned char>(data[i++]); if ((c & 0xc0) != 0x80) return false; cp = (cp << 6) | (c & 63); }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}

// This scanner runs only after a syntax error. yyjson owns JSON validation;
// the scanner recovers the enclosing key/index from its byte offset.
std::string ErrorPath(const char* text, size_t size, size_t stop) {
    struct Frame {
        Frame(bool isObject, const std::string& value) : object(isObject), path(value) {}
        bool object; std::string path, key; size_t index = 0; bool expectKey = true;
    };
    std::vector<Frame> stack;
    auto path = [&]() { if (stack.empty()) return std::string("$"); const auto& f = stack.back();
        return f.object ? KeyPath(f.path, f.key) : f.path + '[' + std::to_string(f.index) + ']'; };
    size_t i = size >= 3 && std::memcmp(text, "\xef\xbb\xbf", 3) == 0 ? 3 : 0;
    while (i < size && i < stop) {
        char c = text[i];
        if (c == '"') {
            size_t begin = i++;
            while (i < size) { if (text[i++] == '"') break; if (text[i-1] == '\\' && i < size) ++i; }
            if (!stack.empty() && stack.back().object && stack.back().expectKey) {
                Document key(yyjson_read(text + begin, i - begin, 0), yyjson_doc_free);
                if (key && yyjson_is_str(yyjson_doc_get_root(key.get()))) {
                    auto* value = yyjson_doc_get_root(key.get());
                    stack.back().key.assign(yyjson_get_str(value), yyjson_get_len(value));
                }
                stack.back().expectKey = false;
            }
            if (i > stop) break;
        } else if (c == '{' || c == '[') {
            std::string child = path(); stack.emplace_back(c == '{', child); ++i;
            // A depth failure is reported at the first disallowed container.
            if (stack.size() > 64) return child;
        } else if (c == '}' || c == ']') { if (!stack.empty()) stack.pop_back(); ++i; }
        else if (c == ',') { if (!stack.empty()) { ++stack.back().index; stack.back().expectKey = true; } ++i; }
        else ++i;
    }
    return path();
}

struct Encoder {
    yyjson_mut_doc* doc; JsonResult& result; bool sorted;
    std::vector<const void*> active;
    yyjson_mut_val* Fail(const std::string& path, const std::string& message) { result.Fail(path, message); return nullptr; }
    yyjson_mut_val* Value(const VarInfo& v, const std::string& path, int depth) {
        switch (v.GetType()) {
        case VAR_INT: return yyjson_mut_sint(doc, v._int);
        case VAR_FLOAT: {
            if (!std::isfinite(v._float)) return Fail(path, "non-finite float");
            auto* number = yyjson_mut_real(doc, v._float);
            return number;
        }
        case VAR_BOOL: return yyjson_mut_bool(doc, v._bl);
        case VAR_NONE: return yyjson_mut_null(doc);
        case VAR_FUN: return Fail(path, "unsupported type: function");
        case VAR_VEC: return Fail(path, "unsupported type: Vector" + std::to_string(v.VectorComponentCount()));
        case VAR_STRING: {
            const auto& s = v._str->_str;
            if (!ValidString(s.data(), s.size())) return Fail(path, "invalid UTF-8 or NUL in string");
            // Serialization is synchronous, with no callbacks or VM mutation.
            return yyjson_mut_strn(doc, s.data(), s.size());
        }
        case VAR_VEC4: return Fail(path, "unsupported type: Vector" + std::to_string(v.VectorComponentCount()));
        case VAR_FP_NATIVE: return Fail(path, "unsupported type: engine object");
        case VAR_ARRAY: return Fail(path, "unsupported type: system.array");
        case VAR_MAP:
        case VAR_LIST: {
            if (++depth > 64) return Fail(path, "container depth exceeds 64");
            const void* identity = v.GetType() == VAR_MAP ? static_cast<void*>(v._tbl) : static_cast<void*>(v._lst);
            if (std::find(active.begin(), active.end(), identity) != active.end()) return Fail(path, "cyclic reference");
            active.push_back(identity);
            yyjson_mut_val* container = v.GetType() == VAR_MAP ? yyjson_mut_obj(doc) : yyjson_mut_arr(doc);
            if (!container) throw std::bad_alloc();
            if (v.GetType() == VAR_MAP) {
                std::vector<const MapData*> members;
                members.reserve(v._tbl->GetCount());
                for (int i = 0; i < v._tbl->_BucketCapa; ++i) {
                    const auto& node = v._tbl->_Bucket[i];
                    if (!IsMapNodeUsed(node)) continue;
                    if (node.data->key.GetType() != VAR_STRING) return Fail(path + "[<key>]", std::string("non-string key: ") + (node.data->key.GetType() == VAR_INT ? "int" : node.data->key.GetType() == VAR_FLOAT ? "float" : node.data->key.GetType() == VAR_BOOL ? "bool" : "unsupported value"));
                    members.push_back(node.data);
                }
                if (sorted) std::sort(members.begin(), members.end(), [](const MapData* a, const MapData* b) {
                    return a->key._str->_str < b->key._str->_str;
                });
                for (const MapData* member : members) {
                    const auto& key = member->key._str->_str;
                    const std::string child = KeyPath(path, key);
                    if (!ValidString(key.data(), key.size())) return Fail(child, "invalid UTF-8 or NUL in key");
                    if (member->value.GetType() == VAR_NONE) continue;
                    auto* value = Value(member->value, child, depth); if (!value) return nullptr;
                    auto* name = yyjson_mut_strn(doc, key.data(), key.size());
                    if (!name || !yyjson_mut_obj_add(container, name, value)) throw std::bad_alloc();
                }
            } else {
                for (int i = 0; i < v._lst->GetCount(); ++i) {
                    auto* item = Value(*v._lst->GetValue(i), path + '[' + std::to_string(i) + ']', depth);
                    if (!item) return nullptr;
                    if (!yyjson_mut_arr_append(container, item)) throw std::bad_alloc();
                }
            }
            active.pop_back(); return container;
        }
        case VAR_SET: return Fail(path, "unsupported type: set");
        case VAR_CLOSURE: return Fail(path, "unsupported type: function");
        default: return Fail(path, "unsupported type: opaque VM value");
        }
    }
};

struct Decoder {
    CNeoVMWorker& worker; JsonResult& result; bool strict;
    bool Number(yyjson_val* value, VarInfo& out, const std::string& path) {
        bool integer = yyjson_is_int(value);
        double number = yyjson_get_num(value);
        if (yyjson_is_sint(value)) {
            int64_t n = yyjson_get_sint(value);
            worker.Var_SetInt(&out, n); return true;
        } else if (yyjson_is_uint(value)) {
            uint64_t n = yyjson_get_uint(value);
            if (n <= uint64_t(INT64_MAX)) { worker.Var_SetInt(&out, static_cast<int64_t>(n)); return true; }
        } else if (yyjson_is_raw(value)) {
            const std::string token(yyjson_get_raw(value), yyjson_get_len(value));
            integer = token.find_first_of(".eE") == std::string::npos;
            if (integer && strict) return result.Fail(path, "integer outside int64 range");
            yyjson_val numeric; yyjson_read_err error;
            if (!yyjson_read_number(token.c_str(), &numeric, 0, nullptr, &error))
                return result.Fail(path, "number outside float64 range");
            number = yyjson_get_num(&numeric);
        }
        if (integer && strict) return result.Fail(path, "integer outside int64 range");
        if (!std::isfinite(number) || std::fabs(number) > (std::numeric_limits<NS_FLOAT>::max)())
            return result.Fail(path, "number outside float64 range");
        if (integer) result.warnings.push_back(path + ": integer outside int64 range converted to float64");
        worker.Var_SetFloat(&out, static_cast<NS_FLOAT>(number)); return true;
    }
    bool Value(yyjson_val* value, VarInfo& out, const std::string& path, int depth) {
        if (yyjson_is_null(value)) return true;
        if (yyjson_is_bool(value)) { worker.Var_SetBool(&out, yyjson_get_bool(value)); return true; }
        if (yyjson_is_num(value) || yyjson_is_raw(value)) return Number(value, out, path);
        if (yyjson_is_str(value)) {
            const char* str = yyjson_get_str(value); size_t len = yyjson_get_len(value);
            if (std::memchr(str, 0, len)) return result.Fail(path, "NUL in string");
            worker.Var_SetStringA(&out, std::string(str, len)); return true;
        }
        if (++depth > 64) return result.Fail(path, "container depth exceeds 64");
        const size_t size = yyjson_get_len(value);
        if (size > static_cast<size_t>(INT32_MAX)) return result.Fail(path, "container exceeds VM index range");
        if (yyjson_is_arr(value)) {
            auto* list = worker.GetVM()->ListAlloc(); worker.Var_SetList(&out, list); list->Resize(static_cast<int>(size));
            size_t i, max; yyjson_val* item;
            yyjson_arr_foreach(value, i, max, item) {
                if (!Value(item, *list->GetValue(static_cast<int>(i)), path + '[' + std::to_string(i) + ']', depth)) return false;
                if (list->GetValue(static_cast<int>(i))->IsContainerType()) list->MarkContainerChild();
            }
        } else {
            auto* map = worker.GetVM()->TableAlloc(static_cast<int>(size)); worker.Var_SetTable(&out, map);
            size_t i, max; yyjson_val *key, *item;
            yyjson_obj_foreach(value, i, max, key, item) {
                const std::string name(yyjson_get_str(key), yyjson_get_len(key));
                const std::string child = KeyPath(path, name);
                if (name.find('\0') != std::string::npos) return result.Fail(child, "NUL in key");
                HeldValue converted(worker), keyValue(worker);
                if (!Value(item, converted.value, child, depth)) return false;
                worker.Var_SetStringA(&keyValue.value, name);
                // Last duplicate wins, including null removing an earlier key.
                if (converted.value.GetType() == VAR_NONE) map->Remove(&keyValue.value);
                else map->Insert(&keyValue.value, &converted.value);
            }
        }
        return true;
    }
};
}

bool Encode(const VarInfo& value, std::string& text, JsonResult& result, bool sortKeys) {
    result = JsonResult(); text.clear();
    try {
        MutableDocument doc(yyjson_mut_doc_new(nullptr), yyjson_mut_doc_free);
        if (!doc) throw std::bad_alloc();
        Encoder encoder{doc.get(), result, sortKeys, {}};
        auto* root = encoder.Value(value, "$", 0);
        if (!root) { if (result.error.empty()) throw std::bad_alloc(); return false; }
        yyjson_mut_doc_set_root(doc.get(), root);
        size_t size = 0; yyjson_write_err error;
        std::unique_ptr<char, decltype(&std::free)> buffer(
            yyjson_mut_write_opts(doc.get(), 0, nullptr, &size, &error), std::free);
        if (!buffer) return result.Fail("$", error.msg ? error.msg : "JSON write failed");
        text.assign(buffer.get(), size); return true;
    } catch (const std::exception& error) { return result.Fail("$", error.what()); }
}
bool Decode(CNeoVMWorker& worker, const char* text, size_t size, VarInfo& value,
    JsonResult& result, bool strictIntegers, bool requireMap) {
    result = JsonResult();
    try {
        yyjson_read_err error;
        Document doc(yyjson_read_opts(const_cast<char*>(text), size,
            YYJSON_READ_ALLOW_BOM | YYJSON_READ_BIGNUM_AS_RAW, nullptr, &error), yyjson_doc_free);
        if (!doc) return result.Fail(ErrorPath(text, size, error.pos),
            std::string(error.msg ? error.msg : "JSON read failed") + " at byte " + std::to_string(error.pos));
        auto* root = yyjson_doc_get_root(doc.get());
        if (requireMap && !yyjson_is_obj(root)) return result.Fail("$", "expected map");
        HeldValue decoded(worker); Decoder decoder{worker, result, strictIntegers};
        if (!decoder.Value(root, decoded.value, "$", 0)) return false;
        // Commit only a complete value. The return slot can alias an input.
        worker.Move(&value, &decoded.value); return true;
    } catch (const std::exception& error) { return result.Fail("$", error.what()); }
}
}}
