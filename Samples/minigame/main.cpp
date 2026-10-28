#include "NeoScript.h"

#include <array>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static std::string ToUtf8(const std::wstring& text)
{
    if (text.empty()) return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Cannot convert path to UTF-8");
    std::string result(size, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.c_str(),
        static_cast<int>(text.size()), &result[0], size, nullptr, nullptr))
        throw std::runtime_error("Cannot convert path to UTF-8");
    return result;
}

static std::wstring FromUtf8(const std::string& text)
{
    if (text.empty()) return std::wstring();
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 path");
    std::wstring result(size, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(),
        static_cast<int>(text.size()), &result[0], size))
        throw std::runtime_error("Invalid UTF-8 path");
    return result;
}
#endif

static std::vector<std::string> CommandLine(int argc, char** argv)
{
#ifdef _WIN32
    // 좁은 argv가 시스템 코드 페이지에서 잃은 경로 문자를 복구한다.
    int count = 0;
    wchar_t** nativeArgs = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!nativeArgs) throw std::runtime_error("Cannot read command line");
    const std::unique_ptr<void, decltype(&LocalFree)> ownedArgs(nativeArgs, &LocalFree);
    std::vector<std::string> args;
    for (int i = 0; i < count; ++i) args.push_back(ToUtf8(nativeArgs[i]));
    (void)argc;
    (void)argv;
    return args;
#else
    return std::vector<std::string>(argv, argv + argc);
#endif
}

static std::string DefaultScript(const std::string& executable)
{
    std::string path;
#ifdef _WIN32
    // 실행 파일의 경로를 사용하므로 작업 폴더와 한글 경로에 영향을 받지 않는다.
    std::vector<wchar_t> buffer(512);
    for (;;)
    {
        const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!size) throw std::runtime_error("Cannot locate executable");
        if (size < buffer.size())
        {
            path = ToUtf8(std::wstring(buffer.data(), size));
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    (void)executable;
    const std::string::size_type separator = path.find_last_of("/\\");
#else
    path = executable;
    if (path.empty() || path[0] != '/')
    {
        std::vector<char> buffer(512);
        while (!getcwd(buffer.data(), buffer.size()))
        {
            if (errno != ERANGE) throw std::runtime_error("Cannot read working directory");
            buffer.resize(buffer.size() * 2);
        }
        path = std::string(buffer.data()) + '/' + path;
    }
    const std::string::size_type separator = path.find_last_of('/');
#endif
    return path.substr(0, separator == std::string::npos ? 0 : separator + 1) + "scripts/mover.ns";
}

static std::FILE* OpenFile(const std::string& path, bool write)
{
#ifdef _WIN32
    return _wfopen(FromUtf8(path).c_str(), write ? L"wb" : L"rb");
#else
    return std::fopen(path.c_str(), write ? "wb" : "rb");
#endif
}

typedef std::unique_ptr<std::FILE, int (*)(std::FILE*)> File;

using namespace NeoScript;

struct GameObject
{
    const char* name;
    std::array<float, 3> position;
    InstanceHandle instance;
    FunctionHandle onUpdate;
};

static bool Fail(CallContext& ctx, const char* message)
{
    ctx.fail(1, message);
    return false;
}

static bool ObjectMethod(CallContext& ctx, StringView method)
{
    auto& object = *static_cast<GameObject*>(ctx.userData());
    if (method.str() != "AddEvent")
        return Fail(ctx, "Unknown GameObject method; this sample supports AddEvent only");
    if (ctx.argCount() != 2 || ctx.argType(0) != ValueType::String
        || ctx.argString(0).str() != "OnUpdate" || ctx.argType(1) != ValueType::Function)
        return Fail(ctx, "Expected GameObject.AddEvent(\"OnUpdate\", function)");
    object.onUpdate = ctx.argFunction(1);
    return object.onUpdate ? true : Fail(ctx, "Invalid update callback");
}

static bool ObjectProperty(CallContext& ctx, StringView property, bool isGet)
{
    auto& object = *static_cast<GameObject*>(ctx.userData());
    if (property.str() == "Name" && isGet)
    {
        ctx.retString(object.name);
        return true;
    }
    if (property.str() != "Position")
        return Fail(ctx, "Expected Position (Vector3) or read-only Name (string)");
    if (isGet)
        ctx.retVec3(object.position[0], object.position[1], object.position[2]);
    else
    {
        if (ctx.argType(0) != ValueType::Vec3)
            return Fail(ctx, "GameObject.Position requires a Vector3, not a list");
        float xyz[3];
        if (!ctx.argVec(0, xyz, 3)) return false;
        for (int i = 0; i < 3; ++i)
        {
            if (!std::isfinite(xyz[i]))
                return Fail(ctx, "GameObject.Position must be finite");
        }
        object.position = {xyz[0], xyz[1], xyz[2]};
    }
    return true;
}

static bool ServicesMethod(CallContext& ctx, StringView method)
{
    if (method.str() != "Log" || ctx.argCount() != 1 || ctx.argType(0) != ValueType::String)
        return Fail(ctx, "Expected Services.Log(string)");
    std::cout << "[script] " << ctx.argString(0).str() << '\n';
    return true;
}

static void BindInstance(IRuntime* runtime, InstanceHandle instance, void* userData)
{
    runtime->BindGlobalObject(instance, "GameObject", runtime->GetObjectType("GameObjectRef"), userData);
    runtime->BindGlobalObject(instance, "Services", runtime->GetObjectType("SceneServices"), nullptr);
}

static std::string ReadScript(const std::string& path)
{
    File file(OpenFile(path, false), &std::fclose);
    if (!file) throw std::runtime_error("Cannot read script: " + path);
    std::string text;
    char buffer[8192];
    std::size_t size;
    while ((size = std::fread(buffer, 1, sizeof(buffer), file.get())) != 0)
        text.append(buffer, size);
    if (std::ferror(file.get())) throw std::runtime_error("Failed while reading script: " + path);
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    return text;
}

static void WriteRecording(const std::string& path, const std::string& frames)
{
    File file(OpenFile(path, true), &std::fclose);
    if (!file) throw std::runtime_error("Cannot write recording: " + path);
    const std::string text = "window.neoScene = {\"fps\":60,\"frames\":[\n" + frames + "\n]};\n";
    if (std::fwrite(text.data(), 1, text.size(), file.get()) != text.size())
        throw std::runtime_error("Cannot write recording: " + path);
    if (std::fclose(file.release()) != 0)
        throw std::runtime_error("Cannot write recording: " + path);
}

class MiniGame
{
public:
    // 네이티브 객체는 주소를 유지하며 스크립트 인스턴스보다 오래 살아 있다.
    std::array<GameObject, 2> objects{{
        {"Red", {-2.0f, 0.0f, -0.5f}, {}, {}},
        {"Blue", {2.0f, 0.5f, 0.5f}, {}, {}}
    }};

    MiniGame() = default;
    MiniGame(const MiniGame&) = delete;
    MiniGame& operator=(const MiniGame&) = delete;

    ~MiniGame()
    {
        if (!runtime) return;
        for (auto& object : objects)
        {
            object.onUpdate = {}; // 인스턴스를 없애기 전에 보관한 콜백을 해제한다.
            if (object.instance) runtime->DestroyInstance(object.instance);
        }
        if (program) runtime->DestroyProgram(program);
        DestroyRuntime(runtime);
    }

    void Load(const std::string& path)
    {
        const std::string source = ReadScript(path);
        RuntimeDesc desc;
        desc.onInstanceBind = BindInstance;
        runtime = CreateRuntime(desc);
        if (!runtime) throw std::runtime_error("Cannot create NeoScript runtime");

        // 컴파일러에 보이는 이름을 선언한 뒤 호출 처리 함수를 등록한다.
        NativeObjectDesc global;
        global.name = "GameObject";
        Register(global);
        global.name = "Services";
        Register(global);
        NativeObjectDesc object;
        object.name = "GameObjectRef";
        object.method = ObjectMethod;
        object.property = ObjectProperty;
        object.declareGlobal = false;
        Register(object);
        NativeObjectDesc services;
        services.name = "SceneServices";
        services.method = ServicesMethod;
        services.declareGlobal = false;
        Register(services);
        runtime->FreezeBindings();

        std::string sourceName = path;
#ifdef _WIN32
        for (char& character : sourceName)
            if (character == '\\') character = '/';
#endif
        CompileDesc compile;
        compile.source = source;
        compile.sourceName = sourceName;
        compile.includeDebugInfo = true;
        const CompileResult result = runtime->Compile(compile);
        if (!result.program) throw std::runtime_error(result.error.message);
        program = result.program; // 한 번 컴파일한 프로그램을 두 객체가 공유한다.

        for (auto& body : objects)
        {
            InstanceDesc instance;
            instance.userData = &body;
            instance.runGlobalInit = false;
            body.instance = runtime->CreateInstance(program, instance);
            if (!body.instance) ThrowLastError("Cannot create script instance");
            // 바인딩이 끝났으므로 최상위 코드에서 콜백을 등록한다.
            if (runtime->RunGlobalInit(body.instance) != RunStatus::Completed)
                ThrowLastError("Global initialization did not complete");
            if (!body.onUpdate) throw std::runtime_error("Script did not register OnUpdate");
        }
    }

    void Update(float dt)
    {
        for (auto& object : objects)
        {
            auto call = runtime->Call(object.instance, object.onUpdate);
            const RunStatus status = call.argFloat(dt).budget(100000).invoke();
            if (status != RunStatus::Completed)
                throw std::runtime_error(std::string(object.name) + ": OnUpdate did not complete. "
                    + call.error().message);
        }
        runtime->CollectCycles(false); // 객체마다가 아니라 호스트 틱마다 한 번 실행한다.
    }

    bool Verify() const
    {
        bool ok = true;
        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            const auto& object = objects[i];
            int32_t updates = 0;
            const float expectedX = i == 0 ? 0.0f : 1.0f;
            const float expectedY = (i == 0 ? 0.0f : 0.5f) + std::sin(6.0f) * 0.5f;
            const float expectedZ = i == 0 ? -0.5f : 0.5f;
            const bool passed = runtime->GetGlobalInt(object.instance, "updates", updates)
                && updates == 120 && std::fabs(object.position[0] - expectedX) < 0.001f
                && std::fabs(object.position[1] - expectedY) < 0.001f
                && std::fabs(object.position[2] - expectedZ) < 0.001f;
            if (!passed) std::cerr << "FAIL: " << object.name << " state or callback count\n";
            ok = passed && ok;
        }
        std::cout << (ok ? "PASS" : "FAIL")
            << ": shared program, two independent instances, 120 updates each\n";
        return ok;
    }

private:
    IRuntime* runtime = nullptr;
    ProgramHandle program;

    void Register(const NativeObjectDesc& desc)
    {
        if (!runtime->RegisterObject(desc)) ThrowLastError("Cannot register native object");
    }

    [[noreturn]] void ThrowLastError(const char* fallback) const
    {
        StringView error;
        throw std::runtime_error(runtime->TakeLastError(error) ? error.str() : fallback);
    }
};

static void PrintFrame(const MiniGame& game, int frame)
{
    std::cout << "frame " << std::setw(3) << frame;
    for (const auto& object : game.objects)
        std::cout << " | " << object.name << " (" << object.position[0] << ", "
            << object.position[1] << ", " << object.position[2] << ')';
    std::cout << '\n';
}

static void RecordFrame(std::ostream& stream, const MiniGame& game, int frame)
{
    if (frame) stream << ",\n";
    stream << '[';
    for (std::size_t i = 0; i < game.objects.size(); ++i)
    {
        if (i) stream << ',';
        const auto& p = game.objects[i].position;
        stream << '[' << p[0] << ',' << p[1] << ',' << p[2] << ']';
    }
    stream << ']';
}

int main(int argc, char** argv)
{
    try
    {
        const std::vector<std::string> args = CommandLine(argc, argv);
        std::string script = DefaultScript(args[0]);
        std::string record;
        bool verify = false;
        for (std::size_t i = 1; i < args.size(); ++i)
        {
            const std::string& arg = args[i];
            if (arg == "--verify") verify = true;
            else if ((arg == "--script" || arg == "--record") && i + 1 < args.size())
            {
                if (arg == "--script") script = args[++i];
                else record = args[++i];
            }
            else if (arg == "--help")
            {
                std::cout << "Usage: neoscript_minigame [--script path] [--record scene-data.js] [--verify]\n"
                    << "Runs 120 fixed 1/60-second ticks. --verify checks the bundled mover script.\n";
                return 0;
            }
            else throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }

        MiniGame game;
        game.Load(script);
        std::ostringstream frames;
        frames << std::setprecision(9);
        RecordFrame(frames, game, 0);
        std::cout << std::fixed << std::setprecision(3);
        PrintFrame(game, 0);
        for (int frame = 1; frame <= 120; ++frame)
        {
            game.Update(1.0f / 60.0f);
            RecordFrame(frames, game, frame);
            if (frame % 30 == 0) PrintFrame(game, frame);
        }
        if (verify && !game.Verify()) return 1;
        if (!record.empty())
        {
            WriteRecording(record, frames.str());
            std::cout << "Recorded " << record << ". Open viewer.html beside this file.\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
