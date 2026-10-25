#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace NeoScript
{
// 요청 문자열은 작업 스레드 호출 동안 유효하다. Send가 끝나면 보관하지 않는다.
struct HttpRequest
{
    std::string method;
    std::string url;
    std::string body;
    std::vector<std::pair<std::string, std::string> > headers;
    int timeoutMs = 30000;
};

struct HttpResponse
{
    // HTTP 상태 코드와 별개로 전송의 완료 여부를 나타낸다.
    bool success = false;
    std::string body;
    std::string error;
};

class IHttpTransport
{
public:
    virtual ~IHttpTransport() {}
    // VM을 재진입하지 않는다. 여러 런타임이 공유하면 동시 호출에 대응해야 한다.
    virtual HttpResponse Send(const HttpRequest& request) = 0;
};
}
