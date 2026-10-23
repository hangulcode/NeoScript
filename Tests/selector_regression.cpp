// Exercise selector consumers as combinations, not only isolated vector writes.
// Every valid case runs with/without debug metadata, directly/from an image,
// and in two independent instances. Malformed input must fail compilation.
#include "NeoScript.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>
using namespace NeoScript;
static float position[4] = {8, 8, 8, 8};
static int propertyGets = 0, propertySets = 0;
static bool Prop(CallContext &c, StringView name, bool get)
{
    const auto n = name.str();
    if (n == "Reads" && get)
    {
        c.retInt(propertyGets);
        return true;
    }
    if (n == "Writes" && get)
    {
        c.retInt(propertySets);
        return true;
    }
    if (n == "Position" || n == "ReadOnly")
    {
        if (get)
        {
            ++propertyGets;
            c.retVec3(position[0], position[1], position[2]);
            return true;
        }
        ++propertySets;
        if (n == "ReadOnly")
            return false;
        float v[4] = {};
        c.argVec(0, v);
        for (int i = 0; i < 3; i++)
            position[i] = v[i];
        return true;
    }
    return false;
}
struct TestCase
{
    std::string group, name, source, error;
};
static std::vector<TestCase> cases;
static std::string Wrap(std::string s, int n)
{
    return std::string(n, '(') + s + std::string(n, ')');
}
static void Add(std::string group, std::string name, std::string source, std::string error = "")
{
    cases.push_back({group, name, source, error});
}
static void BuildCases()
{
    const char *ctors[] = {"math.Vector2(8,8)", "math.Vector3(8,8,8)", "math.Vector4(8,8,8,8)",
                           "math.Quaternion(8,8,8,8)"};
    int dims[] = {2, 3, 4, 4};
    const char *ops[] = {"=", "+=", "-=", "*=", "/=", "post++", "post--", "pre++", "pre--"};
    int changed[] = {2, 10, 6, 16, 4, 9, 7, 9, 7};
    int returned[] = {2, 10, 6, 16, 4, 8, 8, 9, 7};
    for (int ty = 0; ty < 4; ty++)
        for (int component = 0; component < dims[ty]; component++)
            for (int container = 0; container < 6; container++)
            {
                if (container == 5 && ty != 1)
                    continue;
                std::string pre = "import math;import system;var calls=0;var rhs=[2];fun Amount(){calls++;return "
                                  "2;}fun Id(var x){return x;}";
                std::string root;
                switch (container)
                {
                case 0:
                    pre += "var a=" + std::string(ctors[ty]) + ";";
                    root = "a";
                    break;
                case 1:
                    pre += "var a=[" + std::string(ctors[ty]) + "];";
                    root = "a[0]";
                    break;
                case 2:
                    pre += "var a={\"v\":" + std::string(ctors[ty]) + "};";
                    root = "a.v";
                    break;
                case 3:
                    pre += "var a={\"v\":[{\"w\":" + std::string(ctors[ty]) + "}]};";
                    root = "a.v[0].w";
                    break;
                case 4:
                    pre += "var a=[" + std::string(ctors[ty]) + "];fun Source(){return a;}";
                    root = "Source()[0]";
                    break;
                default:
                    root = "Host.Position";
                    break;
                }
                if (container != 5)
                    pre += "var copy=" + root + ";";
                for (int op = 0; op < 9; op++)
                    for (int paren : {0, 1, 3})
                        for (int context = 0; context < 8; context++)
                            for (int rhsMode = 0; rhsMode < 2; rhsMode++)
                            {
                                if (op >= 7 && paren != 0)
                                    continue; // prefix parser has historically required an identifier
                                if (op >= 5 && rhsMode)
                                    continue;
                                if (component != 0 && context > 1)
                                    continue; // all components, full consumers on first component
                                std::string lhs = Wrap(root + "[" + std::to_string(component) + "]", paren);
                                std::string expr;
                                if (op < 5)
                                    expr = lhs + ops[op] + (rhsMode ? "Amount()" : "rhs[0]");
                                else if (op == 5 || op == 6)
                                    expr = lhs + (op == 5 ? "++" : "--");
                                else
                                    expr = (op == 7 ? "++" : "--") + lhs;
                                std::string code = pre;
                                if (context == 2)
                                    code += "fun Work(){return (" + expr + ");}";
                                std::string body = "", check = "true";
                                if (container == 5)
                                    body += "var before=Host.Reads;var writes=Host.Writes;";
                                switch (context)
                                {
                                case 0:
                                    body += expr + ";";
                                    break;
                                case 1:
                                    body += "var result=(" + expr + ");";
                                    break;
                                case 2:
                                    body += "var result=Work();";
                                    break;
                                case 3:
                                    body += "var result=(" + expr + ")+0;";
                                    break;
                                case 4:
                                    body += "var result=Id(" + expr + ");";
                                    break;
                                case 5:
                                    body += "var result=tofloat(" + expr + ");";
                                    break;
                                case 6:
                                    body += "var box=[(" + expr + ")];var result=box[0];";
                                    break;
                                default:
                                    body += "var box={\"r\":(" + expr + ")};var result=box.r;";
                                    break;
                                }
                                if (context)
                                    check += " && result==" + std::to_string(returned[op]);
                                if (container == 5)
                                    check += " && Host.Reads==before+1 && Host.Writes==writes+1";
                                check += " && " + root + "[" + std::to_string(component) +
                                         "]==" + std::to_string(changed[op]);
                                check += " && " + root + "[" + std::to_string((component + 1) % dims[ty]) + "]==8";
                                if (container != 5)
                                    check += " && copy[" + std::to_string(component) + "]==8";
                                check += " && calls==" + std::to_string(rhsMode && op < 5 ? 1 : 0);
                                code += "export fun Test(){" + body + "return " + check + ";}";
                                Add("matrix",
                                    std::to_string(ty) + "/" + std::to_string(component) + "/" +
                                        std::to_string(container) + "/" + ops[op] + "/p" + std::to_string(paren) +
                                        "/c" + std::to_string(context) + "/r" + std::to_string(rhsMode),
                                    code);
                            }
            }
    // Index and parent rebinding; explicit read/write effects; alternate syntactic boundaries.
    for (int p : {0, 1, 2, 5})
    {
        std::string lhs = Wrap("a[i][0]", p);
        Add("effects", "index/p" + std::to_string(p),
            "import math;var i=0;var a=[math.Vector3(1,2,3),math.Vector3(100,200,300)];fun F(){i=1;return 10;}export "
            "fun Test(){" +
                lhs + "+=F();return a[0][0]==1 && a[1][0]==110;}");
        Add("effects", "receiver/p" + std::to_string(p),
            "import math;var a=[math.Vector3(1,2,3)];var old=a;var other=[math.Vector3(7,7,7)];fun F(){a=other;return "
            "10;}export fun Test(){" +
                Wrap("a[0][0]", p) + "+=F();return old[0][0]==1 && other[0][0]==17;}");
        Add("effects", "sibling/p" + std::to_string(p),
            "import math;export fun Test(){var a=[math.Vector3(1,2,3)];" + Wrap("a[0][0]", p) +
                "=++a[0][1];return a[0][0]==3 && a[0][1]==3;}");
        Add("effects", "inner-key/p" + std::to_string(p),
            "import math;var j=0;var a=[math.Vector3(1,2,3)];fun F(){j=1;return 10;}export fun Test(){" +
                Wrap("a[0][j]", p) + "+=F();return a[0][0]==1 && a[0][1]==12;}");
        Add("effects", "key-once/p" + std::to_string(p),
            "import math;var n=0;var a=[math.Vector3(1,2,3)];fun Key(){n++;return 0;}export fun Test(){" +
                Wrap("a[Key()][0]", p) + "+=10;return n==1 && a[0][0]==11;}");
    }
    Add("effects", "map dynamic key",
        "import math;var key=\"x\";var a={\"x\":math.Vector3(1,2,3),\"y\":math.Vector3(7,7,7)};fun "
        "F(){key=\"y\";return 10;}export fun Test(){a[key][0]+=F();return a.x[0]==1 && a.y[0]==17;}");
    Add("effects", "rhs replaces element",
        "import math;var a=[math.Vector3(1,2,3)];fun F(){a[0]=math.Vector3(7,7,7);return 10;}export fun "
        "Test(){a[0][0]+=F();return a[0][0]==17 && a[0][1]==7;}");
    Add("effects", "rhs updates native sibling",
        "fun F(){Host.Position[1]=3;return 10;}export fun Test(){Host.Position[0]+=F();return Host.Position[0]==18 && "
        "Host.Position[1]==3;}");
    Add("control", "loop",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];for(var i in 0,20){(a[0][0])+=2;}return "
        "a[0][0]==41;}");
    Add("control", "if short circuit",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];if(false && ((a[0][0]=8)==8)){return false;}if(true "
        "|| ((a[0][1]=8)==8)){}return a[0][0]==1 && a[0][1]==2;}");
    Add("control", "assignment condition",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];if((a[0][0]+=2)==3){return a[0][0]==3;}return "
        "false;}");
    Add("control", "closure",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];var f=fun(){a[0][1]+=2;};f();return a[0][1]==4;}");
    Add("control", "callee result chain",
        "import math;var a=[math.Vector3(1,2,3)];fun F(){return a;}export fun Test(){(F()[0][0])+=2;return "
        "a[0][0]==3;}");
    Add("control", "anonymous call result",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];var f=fun(){return a;};f()[0][0]+=2;return "
        "a[0][0]==3;}");
    Add("control", "function stored nested",
        "fun F(){return 7;}export fun Test(){var a=[{}];(a[0].f)=F;return a[0].f()==7;}");
    Add("control", "ordinary array",
        "import system;export fun Test(){var a=[system.array(8.0,3)];a[0][1]+=2;return a[0][1]==10 && a[0][0]==8;}");
    Add("control", "map null", "export fun Test(){var a=[{\"k\":1}];(a[0].k)=null;return a[0].len()==0;}");
    Add("control", "list null",
        "export fun Test(){var a=[[1,2,3]];(a[0][1])=null;return a[0].len()==3 && a[0][1]==null;}");
    for (const char *idx : {"-1", "3", "0.0", "true", "\"x\""})
        for (const char *op : {"=", "+=", "-=", "*=", "/=", "%="})
        {
            std::string pre = "import math;var a=[math.Vector3(8,8,8)];var copy=a[0];export fun Test(){(a[0][" +
                              std::string(idx) + "])" + op + "2;return true;}export fun Verify(){return a[0]==copy;}";
            Add("errors", std::string(idx) + op, pre, "vector index out of range");
        }
    for (const char *rhs : {"null", "true", "\"x\"", "[]", "{}", "math.Vector3(1,2,3)"})
        for (const char *op : {"+=", "-=", "*=", "/="})
        {
            Add("errors", std::string(op) + rhs,
                "import math;var a=[math.Vector3(8,8,8)];var copy=a[0];export fun Test(){a[0][0]" + std::string(op) +
                    rhs + ";return true;}export fun Verify(){return a[0]==copy;}",
                "invalid operator");
        }
    Add("errors", "modulo",
        "import math;var a=[math.Vector3(8,8,8)];var copy=a[0];export fun Test(){a[0][0]%=2;return true;}export fun "
        "Verify(){return a[0]==copy;}",
        "invalid operator");
    Add("errors", "readonly",
        "export fun Test(){Host.ReadOnly[0]+=2;return true;}export fun Verify(){return Host.ReadOnly[0]==8;}",
        "cannot write property");

    Add("effects", "list growth before borrow",
        "import math;var a=[math.Vector3(1,2,3)];fun Grow(){for(var i in 0,128)a.append(null);return 4;}"
        "export fun Test(){a[0][0]+=Grow();return a[0][0]==5 && a.len()==129;}");
    Add("effects", "map rehash before borrow",
        "import math;var a={};a.v=math.Vector3(1,2,3);fun Grow(){for(var i in 0,128)a[i]=i;return 4;}"
        "export fun Test(){var x=(a.v[0]+=Grow());return x==5 && a.v[0]==5;}");
    Add("control", "repeated writes and value copy",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];"
        "for(var i in 0,100){a[0][0]+=1;a[0][1]+=2;}var copy=a[0];a[0][2]=9;"
        "return copy==math.Vector3(101,202,3) && a[0]==math.Vector3(101,202,9);}");
    Add("control", "assignment component result stays a value",
        "import math;var a=[math.Vector3(1,2,3)];fun Change(){a[0]=math.Vector3(50,60,70);return 4;}"
        "export fun Test(){var x=(a[0][0]+=2)+Change();return x==7 && a[0][0]==50;}");
    Add("control", "assignment result remains writable",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];"
        "(a[0][0]=4)+=2;return a[0][0]==6;}");
    Add("control", "selector after assignment result",
        "export fun Test(){var a=[[null]];((a[0][0]=[1,2]))[1]=9;return a[0][0][1]==9;}");

    // R-values resolve their receiver before later operands or arguments run.
    Add("order", "binary RHS",
        "import math;var i=0;var a=[math.Vector3(1,2,3),math.Vector3(100,200,300)];fun F(){i=1;return 10;}export fun "
        "Test(){return (a[i][0])+F()==11;}");
    Add("order", "argument RHS",
        "import math;var i=0;var a=[math.Vector3(1,2,3),math.Vector3(100,200,300)];fun F(){i=1;return 10;}fun Id(var "
        "x,var y){return x;}export fun Test(){return Id((a[i][0]),F())==1;}");
    Add("order", "method receiver",
        "var i=0;var a=[[1],[2]];fun F(){i=1;return 10;}export fun Test(){a[i].append(F());return a[0].len()==2 && "
        "a[0][1]==10 && a[1].len()==1;}");
    Add("order", "indexed callee",
        "var i=0;fun First(var x){return 1;}fun Second(var x){return 2;}var a=[[First],[Second]];fun F(){i=1;return "
        "10;}export fun Test(){return a[i][0](F())==1;}");
    Add("order", "unary",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];var b=[[true]];return -(a[0][0])==-1 && "
        "!b[0][0]==false;}");
    Add("order", "nested key",
        "import math;var key=[[0]];var a=[math.Vector3(1,2,3),math.Vector3(100,200,300)];fun F(){key[0][0]=1;return "
        "10;}export fun Test(){a[key[0][0]][0]+=F();return a[0][0]==11 && a[1][0]==100;}");
    Add("order", "parenthesized intermediate",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];(((a)[0]))[1]+=4;return a[0][1]==6;}");
    Add("order", "multiple assignment",
        "import math;export fun Test(){var a=[math.Vector3(1,2,3)];var "
        "b=[math.Vector3(4,5,6)];a[0][0]=b[0][1]=9;return a[0][0]==9 && b[0][1]==9;}");
    Add("order", "discarded getter",
        "export fun Test(){var before=Host.Reads;(Host.Position[0]);return Host.Reads==before+1;}");
    Add("order", "RHS fails before getter",
        "var before=0;fun Bad(){var a=[1];return a[9];}export fun "
        "Test(){before=Host.Reads;Host.Position[0]=Bad();}export fun Verify(){return Host.Reads==before;}",
        "index");
    Add("control", "for bounds",
        "export fun Test(){var a=[[0,4,1]];var n=0;for(var i in a[0][0],a[0][1],a[0][2]){n+=i;}return n==6;}");
    Add("control", "foreach source",
        "export fun Test(){var a=[{\"x\":[1,2,3]}];var n=0;foreach(var v in a[0].x){n+=v;}return n==6;}");
    Add("control", "map postfix key",
        "export fun Test(){var n=8;var a={(n++):7};return a[8]==7 && a[9]==null && n==9;}");
    Add("control", "scalar postfix values",
        "export fun Test(){var n=8;var a=[n++];var b={\"x\":n--};return a[0]==8 && b.x==9 && n==8;}");
    Add("limits", "minimum integer",
        "const MIN=-2147483648;export fun Test(){var a=[-2147483648];var m={(-2147483648):1};return type(MIN)==\"int\" "
        "&& MIN==a[0] && MIN==(-2147483647-1) && m[MIN]==1 && -0x80000000==MIN && -0xffffffff==1;}");

    // Many constants exercise LOADK and writeback's parent/key operands.
    std::string large = "import math;";
    for (int i = 0; i < 15100; i++)
        large += "const k" + std::to_string(i) + "=\"s" + std::to_string(i) + "\";";
    large += "var a={};a[k15099]=math.Vector3(8,8,8);export fun Test(){a[k15099][0]+=2;return a[k15099][0]==10;}";
    Add("limits", "extended constants", large);
}
static int failures = 0, checks = 0;
static std::map<std::string, std::pair<int, int>> counts;
static void Fail(const TestCase &t, int id, bool d, bool image, const std::string &why)
{
    ++failures;
    ++counts[t.group].second;
    std::fprintf(stderr, "FAIL id=%d debug=%d image=%d %s %s: %s\n%s\n", id, d, image, t.group.c_str(), t.name.c_str(),
                 why.c_str(), t.source.c_str());
}
static void Run(IRuntime *rt, const TestCase &t, int id, bool debug, bool image)
{
    const std::string sourceName = t.group + "-" + std::to_string(id) + ".ns";
    CompileDesc cd;
    cd.source = t.source;
    cd.sourceName = sourceName;
    cd.includeDebugInfo = debug;
    ProgramHandle p;
    Error e;
    if (image)
    {
        std::vector<uint8_t> b;
        e = rt->CompileToBytecode(cd, b);
        if (e.ok())
            p = rt->LoadProgram(b, &e);
    }
    else
    {
        auto r = rt->Compile(cd);
        p = r.program;
        e = r.error;
    }
    if (!p)
    {
        ++checks;
        ++counts[t.group].first;
        Fail(t, id, debug, image, "compile: " + e.message);
        return;
    }
    for (int repeat = 0; repeat < 2; repeat++)
    {
        for (float &x : position)
            x = 8;
        propertyGets = propertySets = 0;
        ++checks;
        ++counts[t.group].first;
        auto ins = rt->CreateInstance(p);
        if (!ins)
        {
            Fail(t, id, debug, image, "instance creation");
            continue;
        }
        bool ok = false;
        std::string why;
        {
            auto c = rt->Call(ins, "Test");
            auto status = c.invoke();
            why = c.error().message;
            if (t.error.empty())
                ok = status == RunStatus::Completed && c.retType() == ValueType::Bool && c.retBool();
            else
                ok = status == RunStatus::Failed && why.find(t.error) != std::string::npos;
            if (!ok && why.empty())
                why = "unexpected return value";
        }
        if (ok && !t.error.empty())
        {
            auto c = rt->Call(ins, "Verify");
            ok = c.invoke() == RunStatus::Completed && c.retType() == ValueType::Bool && c.retBool();
            if (!ok)
                why = "state changed after error: " + c.error().message;
        }
        if (!ok)
            Fail(t, id, debug, image, why);
        rt->DestroyInstance(ins);
    }
    rt->DestroyProgram(p);
}
static void RejectMalformed(IRuntime *rt)
{
    std::vector<std::string> bad = {
        "export fun Test(){var a=[[1]];(a[0][0]", "export fun Test(){var a=[[1]];((a[0][0])+=1;}",
        "export fun Test(){var a=[[1]];(a[0][0])))+=1;}", "export fun Test(){var a=[[1]];print(a[0][0]",
        "export fun Test(){var a=[[1]];(a[0][0])"};
    for (int n : {1, 2, 4, 16, 64, 256})
        bad.push_back("export fun Test(){var a=[[1]];" + std::string(n, '(') + "a[0][0]" + std::string(n - 1, ')') +
                      "+=1;}");
    for (int n : {1, 2, 4, 16, 64, 256, 1024})
        bad.push_back("export fun Test(){var a=[[1]];a[0][0]" + std::string(n, ')'));
    const std::string valid = "export fun Test(){var a=[[1]];((a[0][0]))+=1;return a[0][0]==2;}";
    for (size_t i = 1; i < valid.size(); i++)
        bad.push_back(valid.substr(0, i));

    for (bool debug : {false, true})
        for (const auto &source : bad)
        {
            ++checks;
            CompileDesc c;
            c.source = source;
            c.sourceName = "incomplete.ns";
            c.includeDebugInfo = debug;
            auto r = rt->Compile(c);
            if (r.program || r.error.ok())
            {
                ++failures;
                std::fprintf(stderr, "Accepted malformed source (debug=%d): %s\n", debug, source.c_str());
            }
            if (r.program)
                rt->DestroyProgram(r.program);
        }
}
int main()
{
    auto rt = CreateRuntime();
    NativeObjectDesc h;
    h.name = "Host";
    h.property = Prop;
    rt->RegisterObject(h);
    rt->FreezeBindings();
    BuildCases();
    for (bool debug : {false, true})
        for (bool image : {false, true})
            for (int i = 0; i < (int)cases.size(); ++i)
                Run(rt, cases[i], i, debug, image);
    RejectMalformed(rt);
    for (const auto &g : counts)
        std::printf("%s: %d/%d\n", g.first.c_str(), g.second.first - g.second.second, g.second.first);
    DestroyRuntime(rt);
    std::printf("Selector regression %s: %d/%d\n", failures ? "FAIL" : "PASS", checks - failures, checks);
    return failures ? 1 : 0;
}
