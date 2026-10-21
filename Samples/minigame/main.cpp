#include "NeoScript.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
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
        float xyz[4]{};
        ctx.argVec(0, xyz);
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

static std::string ReadScript(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read script: " + path.string());
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) throw std::runtime_error("Failed while reading script: " + path.string());
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    return text;
}

class MiniGame
{
public:
    // Native objects have stable addresses and outlive their script instances.
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
            object.onUpdate = {}; // Drop retained callbacks before destroying instances.
            if (object.instance) runtime->DestroyInstance(object.instance);
        }
        if (program) runtime->DestroyProgram(program);
        DestroyRuntime(runtime);
    }

    void Load(const fs::path& path)
    {
        const std::string source = ReadScript(path);
        RuntimeDesc desc;
        desc.onInstanceBind = BindInstance;
        runtime = CreateRuntime(desc);
        if (!runtime) throw std::runtime_error("Cannot create NeoScript runtime");

        // Declare the names visible to the compiler, then register dispatchers.
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

        const std::string sourceName = path.generic_string();
        CompileDesc compile;
        compile.source = source;
        compile.sourceName = sourceName;
        compile.includeDebugInfo = true;
        const CompileResult result = runtime->Compile(compile);
        if (!result.program) throw std::runtime_error(result.error.message);
        program = result.program; // Compile once; both objects share this program.

        for (auto& body : objects)
        {
            InstanceDesc instance;
            instance.userData = &body;
            instance.runGlobalInit = false;
            body.instance = runtime->CreateInstance(program, instance);
            if (!body.instance) ThrowLastError("Cannot create script instance");
            // Binding has already happened; top-level code registers its callback.
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
        runtime->CollectCycles(false); // Once per host tick, not per object.
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
        fs::path script = fs::absolute(fs::path(argv[0])).parent_path() / "scripts/mover.ns";
        fs::path record;
        bool verify = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--verify") verify = true;
            else if ((arg == "--script" || arg == "--record") && i + 1 < argc)
            {
                if (arg == "--script") script = fs::path(argv[++i]);
                else record = fs::path(argv[++i]);
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
            std::ofstream file(record, std::ios::binary);
            file << "window.neoScene = {\"fps\":60,\"frames\":[\n" << frames.str() << "\n]};\n";
            file.close();
            if (!file) throw std::runtime_error("Cannot write recording: " + record.string());
            std::cout << "Recorded " << record.string() << ". Open viewer.html beside this file.\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
