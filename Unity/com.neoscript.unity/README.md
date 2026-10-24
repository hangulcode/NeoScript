# NeoScript for Unity

This UPM package provides the C# binding, editor integration, tests, and Basic
sample for the NeoScript VM. See the [Unity integration guide](../../docs/Unity.md)
for setup, native plugin builds, platform support, and test commands.

## Update the package and native plugin together

The current binding requires **C ABI 5**. Close Unity and rebuild the native
plugin for each target after updating the package sources. Native binaries are
not included in the repository. The ABI revision is separate from the package
version and compiled script image version.

Layout verification uses the current pointer size, including 32-bit ARMv7.
The ABI 5 update adds safe disposal entry points without changing the C
structure layouts.

## Calls and object lifetime

Finish each `NeoCall` with an `Invoke*` terminal, or call `Abandon()` if it will
not run. `NeoInstance.Dispose()` and `NeoRuntime.Dispose()` throw `NeoException`
while an invocation or executing callback still uses their target. Refusal keeps
the native owner and managed handles intact; finish the call or return from the
callback before retrying. `NeoInstance.Reset()` returns `false` under the same
guard and preserves its original global-initialization setting when it succeeds.

Readers and builders are borrowed. Use `NeoMapReader`, `NeoListReader`, and
`NeoArrayView` only inside their native dispatcher or `InvokeRead*` callback;
copy values that must survive it. Direct C ABI return readers, their children,
and key descriptor arrays belong to the issuing `NsCall`. Calls on different
instances may interleave and end in either order. Repeated reads in one call
preserve existing readers; ending that call or starting another call on the
same instance ends the borrowed return lifetime.

Native objects shared with another instance, including objects inside maps or
lists, retain their source instance identity. Resetting or destroying the source
invalidates old native aliases; subsequent method/property access fails safely.
Fetch fresh objects after reset. Host `userData` remains host-owned. Object-type
tokens are valid only for their owning runtime and expire when it is destroyed.

## Vectors and quaternions

Vector2/3/4 helpers preserve component order. Script quaternions use **w,x,y,z**;
Unity uses **x,y,z,w**. `ArgQuaternion`, `NeoResult.AsQuaternion`, and
`Return(Quaternion)` perform the reorder in the appropriate direction.

Raw Vector4 helpers and vector-array views retain script order even for
quaternion values. `math.Quaternion(1,2,3,4)` therefore becomes
`new Quaternion(2,3,4,1)` through a Quaternion helper, while its raw Vector4
components remain `(1,2,3,4)`.
