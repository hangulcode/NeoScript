using System;
using UnityEngine;
namespace NeoScript.Tests
{
    // 네이티브와 관리 코드의 모든 벡터 차원 및 쿼터니온 순서를 함께 검사한다.
    internal sealed class NeoVectorApiTest : INeoObject
    {
        public bool Invoke(NeoCallContext c, NeoName name)
        {
            if (name.Is("V2")) { c.Return(c.ArgVector2(0)); return true; }
            if (name.Is("V3")) { c.Return(c.ArgVector3(0)); return true; }
            if (name.Is("V4")) { c.Return(c.ArgVector4(0)); return true; }
            if (name.Is("Q")) { c.Return(c.ArgQuaternion(0)); return true; }
            if (name.Is("Short"))
            {
                c.Return(!c.TryArgVector3(0,out Vector3 v) && v==default &&
                    !c.TryArgQuaternion(0,out Quaternion q) && q.Equals(default(Quaternion)) &&
                    !c.TryArgVector2(99,out Vector2 missing) && missing==default);
                return true;
            }
            if (name.Is("Build"))
            {
                var m=c.ReturnMap(); m.Set("a",new Vector2(1,2)); m.Set("b",new Vector3(1,2,3));
                m.Set("c",new Vector4(1,2,3,4)); m.Set("q",new Quaternion(2,3,4,1));
                var l=m.SetList("items");l.Push(new Vector2(1,2));l.Push(new Vector3(1,2,3));
                l.Push(new Vector4(1,2,3,4));l.Push(new Quaternion(2,3,4,1));
                if(!l.TrySet(0,new Vector2(1,2)) || !l.TrySet(1,new Vector3(1,2,3)) ||
                    !l.TrySet(2,new Vector4(1,2,3,4)) || !l.TrySet(3,new Quaternion(2,3,4,1)) ||
                    l.TrySet(4,new Vector2(1,2))) throw new Exception("list vector setters");
                return true;
            }
            if (name.Is("ArrayQ"))
            {
                var a=c.ArgArray(0);bool ok=a.TryGet(0,out Quaternion q)&&q.Equals(new Quaternion(2,3,4,1));
                c.Return(ok&&a.TrySet(1,q));return true;
            }
            return false;
        }
        internal static void Run()
        {
            using(var rt=new NeoRuntime())
            {
                rt.RegisterObject("Vectors",new NeoVectorApiTest());rt.Freeze();
                using(var p=rt.Compile(@"import math;import system;
export fun V2(var v){return Vectors.V2(v);} export fun V3(var v){return Vectors.V3(v);}
export fun V4(var v){return Vectors.V4(v);} export fun Q(var v){return Vectors.Q(v);}
export fun Short(){return Vectors.Short(math.Vector2(1,2));}
export fun Build(){return Vectors.Build();}
export fun ArrayQ(){var a=system.array(math.Quaternion(1,2,3,4),2);return Vectors.ArrayQ(a)&&a[1]==math.Quaternion(1,2,3,4);}","vector-api.ns"))
                using(var i=p.CreateInstance())
                {
                    Require(i.Call("V2").Arg(new Vector2(1,2)).InvokeVector2()==new Vector2(1,2),"Vec2");
                    Require(i.Call("V3").Arg(new Vector3(1,2,3)).InvokeVector3()==new Vector3(1,2,3),"Vec3");
                    Require(i.Call("V4").Arg(new Vector4(1,2,3,4)).InvokeVector4()==new Vector4(1,2,3,4),"Vec4");
                    Require(i.Call("Q").Arg(new Quaternion(2,3,4,1)).InvokeQuaternion().Equals(new Quaternion(2,3,4,1)),"Quaternion wxyz");
                    Require(i.Call("V2").Arg(new Vector4(1,2,3,4)).InvokeVector2()==new Vector2(1,2),"prefix read");
                    Require(i.Call("Short").InvokeBool(),"short vector fails safely");
                    Require(i.Call("ArrayQ").InvokeBool(),"quaternion array");
                    i.Call("Build").InvokeReadMap(m=>
                    {
                        Require(m.TryGet("a",out Vector2 a)&&a==new Vector2(1,2),"map Vec2");
                        Require(m.TryGet("b",out Vector3 b)&&b==new Vector3(1,2,3),"map Vec3");
                        Require(m.TryGet("c",out Vector4 c)&&c==new Vector4(1,2,3,4),"map Vec4");
                        Require(m.TryGet("q",out Quaternion q)&&q.Equals(new Quaternion(2,3,4,1)),"map Quaternion");
                        Require(!m.TryGet("a",out Vector3 _)&&!m.TryGet("missing",out Quaternion _),"map short/missing");
                        var l=m.GetList("items");
                        Require(l.TryGet(0,out Vector2 x)&&x==new Vector2(1,2),"list Vec2");
                        Require(l.TryGet(1,out Vector3 y)&&y==new Vector3(1,2,3),"list Vec3");
                        Require(l.TryGet(2,out Vector4 z)&&z==new Vector4(1,2,3,4),"list Vec4");
                        Require(l.TryGet(3,out Quaternion w)&&w.Equals(new Quaternion(2,3,4,1)),"list Quaternion");
                        Require(!l.TryGet(0,out Vector3 _)&&!l.TryGet(-1,out Vector2 _),"list short/missing");
                    });
                }
            }
        }
        private static void Require(bool ok,string name){if(!ok)throw new Exception("Vector API: "+name);}
    }
}
