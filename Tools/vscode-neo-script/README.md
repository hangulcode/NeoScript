# Neo Script VS Code Extension

This extension adds `.ns` language support and a Debug Adapter Protocol client for Neo Script.

## Debugging

Build the console sample first so the adapter executable exists:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe" Samples\console\console.sln /p:Configuration=Release /p:Platform=x64 /m
```

Then use this launch configuration:

```json
{
  "type": "neo-script",
  "request": "launch",
  "name": "Debug Neo Script",
  "program": "${file}",
  "cwd": "${workspaceFolder}"
}
```

The extension searches for `Samples/console/x64/Release/console.exe` and starts it with `--dap`.
If your adapter lives elsewhere, set `neoScript.debugAdapterPath` or add `adapterPath` to `launch.json`.

You can also run `Neo Script: Create Launch Config` from the Command Palette while a workspace is open.

Supported debugger actions:

- Breakpoints
- Continue, pause
- Step into, step over, step out
- Call stack
- Variables
- Runtime exception stop and `exceptionInfo`

## Editing

The extension includes TextMate highlighting and snippets for common Neo Script patterns:

- `fun`
- `var`
- `for`
- `foreach`
- `if`
- `class`
- `import`
- `print`
