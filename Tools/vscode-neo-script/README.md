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
  "cwd": "${workspaceFolder}",
  "libPath": "${workspaceFolder}\\Lib"
}
```

The extension searches for `Samples/console/x64/Release/console.exe` and starts it with `--dap`.
If your adapter lives elsewhere, set `neoScript.debugAdapterPath` or add `adapterPath` to `launch.json`.
If your Neo Script `Lib` directory lives elsewhere, set `libPath` in `launch.json`.

You can also run `Neo Script: Create Launch Config` from the Command Palette while a workspace is open.
The debugger runs the selected `.ns` file as a top-level script. Put the code you want to debug in the file body, or call exported functions from that top-level script.

The same console executable can also run a script directly outside the debugger:

```powershell
Samples\console\x64\Release\console.exe --file path\to\script.ns
```

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
