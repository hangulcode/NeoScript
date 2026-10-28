# Sized native vector access

Scope: C++ host API, flat C ABI, and Unity bindings.

Last verified: 2026-10-29.

Verification: `ctest --test-dir .build/vector-width/build -C Release -R vector_api --output-on-failure`.

## Contract

Use the sized overload when the host consumes only two or three components:

```cpp
float xy[2];
if (!ctx.argVec(0, xy, 2)) return false;
float xyz[3];
if (!ctx.argVec(1, xyz, 3)) return false;
float rgba[4];
if (!ctx.argVec(2, rgba, 4)) return false;
float wxyz[4];
if (!ctx.argQuat(3, wxyz)) return false;
```

- Requested dimension is 2, 3, or 4. A larger source supplies its prefix.
- Missing arguments/keys, invalid indices, non-vectors, short vectors, null pointers,
  or invalid dimensions return `false` without writing the output.
- The caller allocates at least the requested number of floats. Pointer capacity cannot
  be inferred by the API. Unused lanes are neither read nor written.
- Generic writers create exactly the specified vector dimension. A rejected write does
  not change its destination; a rejected argument/push appends nothing.
- Fixed vector arrays require an exact dimension when writing. Sized native array reads
  may read a prefix; existing typed Unity array reads retain their exact-type checks.
- Quaternion is a four-component value in **w,x,y,z** order. Unity converts to/from its
  **x,y,z,w** constructor and fields. There is no additional VM quaternion type.
- These changes add host entry points, not instructions or a new bytecode format.

Sized reads are available on `CallContext`, `Invocation`, `CallResult`, `MapReader`,
`ListReader`, and `ArrayView`. Sized writes are available on `CallContext`, `Invocation`,
`MapBuilder`, `ListBuilder`, and `ArrayView`; quaternion helpers select dimension four.
Fixed `retVec2/3/4`, `argVec2/3/4`, and builder scalar-coordinate helpers remain available.
Use the fixed helpers when individual scalar coordinates are already available.

`Invocation::invoke()` keeps the return slot alive until the call scope ends.
`invokeR()` snapshots the result and immediately returns its execution context;
read that snapshot with `CallResult::asVec(out, dimension)`, not `Invocation::retVec`.

## Compatibility

Unsized `float out[4]` overloads and existing C exports preserve their previous
four-component/padding behavior. Do not pass a two-float buffer to an unsized reader.
New C exports use `VecN` for sized readers and a dimension argument for generic writers;
rebuild the native plugin with the managed binding, which now requires **C ABI 8**.
Existing C structure layouts are unchanged from ABI 7.

Unity `ArgVector2/3/4` and `ArgQuaternion` now reject short/non-vector arguments with an
exception handled by the binding's callback boundary. Use `TryArgVector*` or
`TryArgQuaternion` for expected failures. Map/list typed `TryGet` returns false for a
short vector instead of silently padding it. Owned `NeoResult.AsVector*` retains its
legacy snapshot conversion behavior. Vector2/4/quaternion invocation, collection writes,
and reads are now exposed alongside the existing Vector3 APIs.

## Audit and optimization decisions

| Surface | Change or reason to retain work |
| --- | --- |
| Argument and collection vector reads | Copy only requested components; no preceding zero fill |
| Generic return/argument writers | Write actual dimensions, reuse existing vector storage through existing COW rules |
| Vector map writes | Write into the map slot instead of allocating/copying/releasing a temporary vector |
| Vector list replacement | Replace the existing element directly through COW-aware setters |
| Native vector-array writes | One overlap-safe component copy instead of zeroed four-float temporary plus full copy |
| Owned call result / C ABI result | Avoid intermediate four-float scratch; retain the four defined C ABI result lanes and snapshot defaults |
| Unity marshaling | Exact-sized stack buffers for reads and generic writers; fixed-coordinate writers avoid scratch entirely |
| String readers | Existing scoped `StringView` is already a view; owned results and UTF-8 transitions still require ownership/encoding work |
| Numeric array views | Existing typed pointers/spans are direct views; narrowing double to float remains an explicit host decision |
| JSON and callback handles | Validation, ownership, and lifetime work is necessary; it is not redundant vector padding |

The VM's internal vector-array stride remains four floats. Its fixed-width moves and
owned result defaults have separate storage/compatibility purposes. This change does not
assume that replacing every 16-byte VM move with a variable-width move is faster.
No runtime speedup percentage is claimed without a controlled benchmark.

## Verification boundaries

Native regression covers dimensions 2/3/4, quaternion order, prefix reads, exact array
writes, short/invalid inputs, unchanged output canaries, legacy padding, snapshots,
allocation cleanup, source compilation and bytecode loading with/without debug info.
C ABI tests cover the same size contract. Managed regression uses the rebuilt DLL and
Unity vector assemblies. Full Unity editor/PlayMode execution still requires a valid
Unity Editor license; compile/standalone managed checks do not replace those tests.
