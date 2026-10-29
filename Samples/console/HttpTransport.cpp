#include "HttpTransport.h"
#include <curl/curl.h>
#include <climits>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace
{
struct CurlGlobal
{
    std::once_flag once;
    bool initialized = false;
    CurlGlobal()
    {
#ifndef _WIN32
        // 외부 libcurl을 쓰는 플랫폼은 초기화의 스레드 안전성을 가정하지 않는다.
        EnsureInitialized();
#endif
    }
    void EnsureInitialized()
    {
        // Windows 콘솔의 고정 libcurl은 SRWLOCK으로 전역 초기화를 보호한다.
        // HTTP를 쓰지 않는 실행에서는 Winsock 등의 초기화 비용을 지불하지 않는다.
        std::call_once(once, [this]() {
            if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
                throw std::runtime_error("libcurl initialization failed");
            initialized = true;
        });
    }
    ~CurlGlobal() { if (initialized) curl_global_cleanup(); }
};

size_t Receive(char* data, size_t size, size_t count, void* user)
{
    std::string& body = *static_cast<std::string*>(user);
    if (size != 0 && count > size_t(INT_MAX) / size) return 0;
    const size_t bytes = size * count;
    if (bytes > size_t(INT_MAX) - body.size()) return 0;
    try { body.append(data, bytes); }
    catch (...) { return 0; }
    return bytes;
}

int Progress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<const NeoScript::HttpRequest*>(user)->IsCancelled() ? 1 : 0;
}

void Check(CURLcode code)
{
    if (code != CURLE_OK) throw std::runtime_error(curl_easy_strerror(code));
}

class CurlTransport : public NeoScript::IHttpTransport
{
    std::shared_ptr<CurlGlobal> global;
public:
    explicit CurlTransport(const std::shared_ptr<CurlGlobal>& owner) : global(owner) {}
    NeoScript::HttpResponse Send(const NeoScript::HttpRequest& input) override
    {
        NeoScript::HttpResponse result;
        try
        {
            if (input.url.find('\0') != std::string::npos || input.timeoutMs < -1)
                throw std::runtime_error("Invalid HTTP URL or timeout");
            if (input.IsCancelled()) throw std::runtime_error("HTTP request cancelled");
            global->EnsureInitialized();
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
            if (!handle) throw std::runtime_error("libcurl handle allocation failed");
            std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, curl_slist_free_all);
            for (const auto& item : input.headers)
            {
                if (item.first.empty() || item.first.find_first_of(":\r\n") != std::string::npos
                    || item.second.find_first_of("\r\n") != std::string::npos
                    || item.first.find('\0') != std::string::npos || item.second.find('\0') != std::string::npos)
                    throw std::runtime_error("Invalid HTTP header");
                const std::string line = item.first + (item.second.empty() ? ";" : ": " + item.second);
                curl_slist* next = curl_slist_append(headers.get(), line.c_str());
                if (!next) throw std::bad_alloc();
                headers.release();
                headers.reset(next);
            }
            char error[CURL_ERROR_SIZE] = {};
            Check(curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, error));
            Check(curl_easy_setopt(handle.get(), CURLOPT_URL, input.url.c_str()));
            Check(curl_easy_setopt(handle.get(), CURLOPT_PROTOCOLS_STR, "http,https"));
            Check(curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L));
            Check(curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT_MS,
                input.timeoutMs == -1 ? 0L : long(input.timeoutMs == 0 ? 1 : input.timeoutMs)));
            Check(curl_easy_setopt(handle.get(), CURLOPT_NOPROGRESS, 0L));
            Check(curl_easy_setopt(handle.get(), CURLOPT_XFERINFOFUNCTION, Progress));
            Check(curl_easy_setopt(handle.get(), CURLOPT_XFERINFODATA, &input));
            Check(curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 1L));
            Check(curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 2L));
            Check(curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, Receive));
            Check(curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &result.body));
            Check(curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get()));
            if (input.method == "POST")
            {
                Check(curl_easy_setopt(handle.get(), CURLOPT_POST, 1L));
                Check(curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE_LARGE, curl_off_t(input.body.size())));
                Check(curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, input.body.c_str()));
            }
            else if (input.method != "GET") throw std::runtime_error("Unsupported HTTP method");
            const CURLcode code = curl_easy_perform(handle.get());
            if (input.IsCancelled()) throw std::runtime_error("HTTP request cancelled");
            if (code != CURLE_OK) throw std::runtime_error(error[0] ? error : curl_easy_strerror(code));
            result.success = true;
        }
        catch (const std::exception& error)
        {
            result.body.clear();
            result.error = error.what();
        }
        return result;
    }
};
}

std::shared_ptr<NeoScript::IHttpTransport> CreateConsoleHttpTransport()
{
    // 모든 전송 객체가 초기화 상태를 공유하고 마지막 전송이 끝날 때까지 보유한다.
    static const std::shared_ptr<CurlGlobal> global = std::make_shared<CurlGlobal>();
    return std::make_shared<CurlTransport>(global);
}
