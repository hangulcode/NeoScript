#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;

// DAP 메시지를 읽어들이는 함수 (std::cin을 통해 읽음)
std::string readDAPMessage() {
	std::string header;
	size_t contentLength = 0;

	// 헤더 부분 읽기: 빈 줄이 나올 때까지 읽음.
	while (std::getline(std::cin, header) && !header.empty()) {
		// "Content-Length:" 헤더 파싱
		const std::string prefix = "Content-Length: ";
		if (header.compare(0, prefix.size(), prefix) == 0) {
			contentLength = std::stoi(header.substr(prefix.size()));
		}
	}

	// contentLength 만큼의 본문(JSON payload) 읽기
	std::string content(contentLength, '\0');
	std::cin.read(&content[0], contentLength);
	return content;
}

// DAP 메시지를 전송하는 함수 (std::cout을 통해 보냄)
void sendDAPMessage(const std::string& jsonPayload) {
	std::cout << "Content-Length: " << jsonPayload.size() << "\r\n\r\n" << jsonPayload;
	std::cout.flush();
}

int main() 
{
	// 디버그 어댑터의 기본 실행 루프
	while (true) {
		std::string message = readDAPMessage();
		if (message.empty()) {
			// 입력이 없으면 종료 (또는 다른 종료 조건 처리)
			break;
		}

		// RapidJSON을 사용하여 JSON 파싱
		Document doc;
		doc.Parse(message.c_str());
		if (doc.HasParseError()) {
			// 파싱 오류 처리 (필요시 로그 출력 등)
			continue;
		}

		// 요청(request) 메시지인지 확인 (예: "command" 필드가 존재)
		if (doc.HasMember("command")) {
			std::string command = doc["command"].GetString();

			if (command == "initialize") {
				// initialize 요청에 대한 응답 생성
				Document response;
				response.SetObject();
				Document::AllocatorType& allocator = response.GetAllocator();

				response.AddMember(StringRef("type"), StringRef("response"), allocator);
				response.AddMember(StringRef("request_seq"), doc["seq"].GetInt(), allocator);
				response.AddMember(StringRef("success"), true, allocator);
				response.AddMember(StringRef("command"), StringRef("initialize"), allocator);

				// capabilities(지원 기능) 설정
				Value body(kObjectType);
				body.AddMember(StringRef("supportsConfigurationDoneRequest"), true, allocator);
				// 필요에 따라 다른 capabilities도 추가 가능

				response.AddMember(StringRef("body"), body, allocator);

				// JSON 문자열로 변환 후 전송
				StringBuffer buffer;
				Writer<StringBuffer> writer(buffer);
				response.Accept(writer);
				sendDAPMessage(buffer.GetString());
			}
			else if (command == "setBreakpoints") {
				// setBreakpoints 요청 처리
				// 예시: 요청의 arguments.breakpoint 배열에서 중단점을 파싱
				// 실제로는 중단점 정보를 내부적으로 저장하여 실행 흐름에 반영해야 함.
				Document response;
				response.SetObject();
				Document::AllocatorType& allocator = response.GetAllocator();

				response.AddMember(StringRef("type"), StringRef("response"), allocator);
				response.AddMember(StringRef("request_seq"), doc["seq"].GetInt(), allocator);
				response.AddMember(StringRef("success"), true, allocator);
				response.AddMember(StringRef("command"), StringRef("setBreakpoints"), allocator);

				Value body(kObjectType);
				// 받은 중단점 정보를 그대로 echo (실제 구현에서는 처리 후 상태를 보냄)
				if (doc.HasMember("arguments") && doc["arguments"].HasMember("breakpoints")) {
					const Value& breakpoints = doc["arguments"]["breakpoints"];
					// 원본 Document의 수명이 끝난 후에도 사용하려면 deep copy가 필요합니다.
					Value breakpointsCopy(breakpoints, allocator);
					body.AddMember(StringRef("breakpoints"), breakpointsCopy, allocator);
				}

				response.AddMember(StringRef("body"), body, allocator);

				StringBuffer buffer;
				Writer<StringBuffer> writer(buffer);
				response.Accept(writer);
				sendDAPMessage(buffer.GetString());
			}
			// 필요에 따라 다른 명령(request) 처리 (예: continue, pause 등)
		}

		// ----------------------------------------------------------------
		// 여기서는 단순히 메시지 처리 후 예시로 중단점 hit 이벤트를 발생시키는 코드
		// 실제 디버거라면 프로그램 실행 흐름에 따라 중단점에 도달했을 때 아래 이벤트를 전송해야 함.
		// ----------------------------------------------------------------

		{
			Document eventDoc;
			eventDoc.SetObject();
			Document::AllocatorType& allocator = eventDoc.GetAllocator();
			eventDoc.AddMember(StringRef("type"), StringRef("event"), allocator);
			eventDoc.AddMember(StringRef("event"), StringRef("stopped"), allocator);
			// seq는 이벤트 번호 (실제 구현에서는 증가시키거나 관리 필요)
			eventDoc.AddMember(StringRef("seq"), 1, allocator);

			Value body(kObjectType);
			body.AddMember(StringRef("reason"), StringRef("breakpoint"), allocator);
			body.AddMember(StringRef("threadId"), 1, allocator);  // 예: 단일 스레드 환경
			eventDoc.AddMember(StringRef("body"), body, allocator);

			StringBuffer eventBuffer;
			Writer<StringBuffer> eventWriter(eventBuffer);
			eventDoc.Accept(eventWriter);
			sendDAPMessage(eventBuffer.GetString());
		}
	}
	return 0;
}