#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;

// DAP �޽����� �о���̴� �Լ� (std::cin�� ���� ����)
std::string readDAPMessage() {
	std::string header;
	size_t contentLength = 0;

	// ��� �κ� �б�: �� ���� ���� ������ ����.
	while (std::getline(std::cin, header) && !header.empty()) {
		// "Content-Length:" ��� �Ľ�
		const std::string prefix = "Content-Length: ";
		if (header.compare(0, prefix.size(), prefix) == 0) {
			contentLength = std::stoi(header.substr(prefix.size()));
		}
	}

	// contentLength ��ŭ�� ����(JSON payload) �б�
	std::string content(contentLength, '\0');
	std::cin.read(&content[0], contentLength);
	return content;
}

// DAP �޽����� �����ϴ� �Լ� (std::cout�� ���� ����)
void sendDAPMessage(const std::string& jsonPayload) {
	std::cout << "Content-Length: " << jsonPayload.size() << "\r\n\r\n" << jsonPayload;
	std::cout.flush();
}

int main() 
{
	// ����� ������� �⺻ ���� ����
	while (true) {
		std::string message = readDAPMessage();
		if (message.empty()) {
			// �Է��� ������ ���� (�Ǵ� �ٸ� ���� ���� ó��)
			break;
		}

		// RapidJSON�� ����Ͽ� JSON �Ľ�
		Document doc;
		doc.Parse(message.c_str());
		if (doc.HasParseError()) {
			// �Ľ� ���� ó�� (�ʿ�� �α� ��� ��)
			continue;
		}

		// ��û(request) �޽������� Ȯ�� (��: "command" �ʵ尡 ����)
		if (doc.HasMember("command")) {
			std::string command = doc["command"].GetString();

			if (command == "initialize") {
				// initialize ��û�� ���� ���� ����
				Document response;
				response.SetObject();
				Document::AllocatorType& allocator = response.GetAllocator();

				response.AddMember(StringRef("type"), StringRef("response"), allocator);
				response.AddMember(StringRef("request_seq"), doc["seq"].GetInt(), allocator);
				response.AddMember(StringRef("success"), true, allocator);
				response.AddMember(StringRef("command"), StringRef("initialize"), allocator);

				// capabilities(���� ���) ����
				Value body(kObjectType);
				body.AddMember(StringRef("supportsConfigurationDoneRequest"), true, allocator);
				// �ʿ信 ���� �ٸ� capabilities�� �߰� ����

				response.AddMember(StringRef("body"), body, allocator);

				// JSON ���ڿ��� ��ȯ �� ����
				StringBuffer buffer;
				Writer<StringBuffer> writer(buffer);
				response.Accept(writer);
				sendDAPMessage(buffer.GetString());
			}
			else if (command == "setBreakpoints") {
				// setBreakpoints ��û ó��
				// ����: ��û�� arguments.breakpoint �迭���� �ߴ����� �Ľ�
				// �����δ� �ߴ��� ������ ���������� �����Ͽ� ���� �帧�� �ݿ��ؾ� ��.
				Document response;
				response.SetObject();
				Document::AllocatorType& allocator = response.GetAllocator();

				response.AddMember(StringRef("type"), StringRef("response"), allocator);
				response.AddMember(StringRef("request_seq"), doc["seq"].GetInt(), allocator);
				response.AddMember(StringRef("success"), true, allocator);
				response.AddMember(StringRef("command"), StringRef("setBreakpoints"), allocator);

				Value body(kObjectType);
				// ���� �ߴ��� ������ �״�� echo (���� ���������� ó�� �� ���¸� ����)
				if (doc.HasMember("arguments") && doc["arguments"].HasMember("breakpoints")) {
					const Value& breakpoints = doc["arguments"]["breakpoints"];
					// ���� Document�� ������ ���� �Ŀ��� ����Ϸ��� deep copy�� �ʿ��մϴ�.
					Value breakpointsCopy(breakpoints, allocator);
					body.AddMember(StringRef("breakpoints"), breakpointsCopy, allocator);
				}

				response.AddMember(StringRef("body"), body, allocator);

				StringBuffer buffer;
				Writer<StringBuffer> writer(buffer);
				response.Accept(writer);
				sendDAPMessage(buffer.GetString());
			}
			// �ʿ信 ���� �ٸ� ���(request) ó�� (��: continue, pause ��)
		}

		// ----------------------------------------------------------------
		// ���⼭�� �ܼ��� �޽��� ó�� �� ���÷� �ߴ��� hit �̺�Ʈ�� �߻���Ű�� �ڵ�
		// ���� ����Ŷ�� ���α׷� ���� �帧�� ���� �ߴ����� �������� �� �Ʒ� �̺�Ʈ�� �����ؾ� ��.
		// ----------------------------------------------------------------

		{
			Document eventDoc;
			eventDoc.SetObject();
			Document::AllocatorType& allocator = eventDoc.GetAllocator();
			eventDoc.AddMember(StringRef("type"), StringRef("event"), allocator);
			eventDoc.AddMember(StringRef("event"), StringRef("stopped"), allocator);
			// seq�� �̺�Ʈ ��ȣ (���� ���������� ������Ű�ų� ���� �ʿ�)
			eventDoc.AddMember(StringRef("seq"), 1, allocator);

			Value body(kObjectType);
			body.AddMember(StringRef("reason"), StringRef("breakpoint"), allocator);
			body.AddMember(StringRef("threadId"), 1, allocator);  // ��: ���� ������ ȯ��
			eventDoc.AddMember(StringRef("body"), body, allocator);

			StringBuffer eventBuffer;
			Writer<StringBuffer> eventWriter(eventBuffer);
			eventDoc.Accept(eventWriter);
			sendDAPMessage(eventBuffer.GetString());
		}
	}
	return 0;
}