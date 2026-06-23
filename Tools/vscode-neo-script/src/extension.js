const fs = require("fs");
const fsp = require("fs/promises");
const path = require("path");
const vscode = require("vscode");

function resolveVariables(value, folder) {
  if (!value || typeof value !== "string") {
    return value;
  }

  const workspaceFolder = folder ? folder.uri.fsPath : "";
  const editor = vscode.window.activeTextEditor;
  const file = editor ? editor.document.uri.fsPath : "";
  return value
    .replace(/\$\{workspaceFolder\}/g, workspaceFolder)
    .replace(/\$\{file\}/g, file);
}

function findAdapterPath(context, folder, explicitPath) {
  const configuredPath = explicitPath || vscode.workspace.getConfiguration("neoScript").get("debugAdapterPath");
  const resolvedConfiguredPath = resolveVariables(configuredPath, folder);
  if (resolvedConfiguredPath && fs.existsSync(resolvedConfiguredPath)) {
    return resolvedConfiguredPath;
  }

  const workspaceFolder = folder ? folder.uri.fsPath : undefined;
  const candidates = [
    workspaceFolder && path.join(workspaceFolder, "Samples", "console", "x64", "Release", "console.exe"),
    workspaceFolder && path.join(workspaceFolder, "Samples", "console", "x64", "Debug", "console.exe"),
    path.join(context.extensionPath, "bin", "console.exe"),
    path.join(context.extensionPath, "bin", "NeoDebugAdapter.exe")
  ].filter(Boolean);

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return resolvedConfiguredPath || candidates[0];
}

async function pathExists(filePath) {
  try {
    await fsp.access(filePath);
    return true;
  } catch (_) {
    return false;
  }
}

function getLaunchTemplate() {
  return {
    type: "neo-script",
    request: "launch",
    name: "Debug Neo Script",
    program: "${file}",
    cwd: "${workspaceFolder}"
  };
}

async function createLaunchConfig() {
  const folder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
  if (!folder) {
    vscode.window.showErrorMessage("Open a workspace folder before creating a Neo Script launch config.");
    return;
  }

  const vscodeDir = path.join(folder.uri.fsPath, ".vscode");
  const launchPath = path.join(vscodeDir, "launch.json");
  await fsp.mkdir(vscodeDir, { recursive: true });

  const template = getLaunchTemplate();
  let launch = { version: "0.2.0", configurations: [template] };

  if (await pathExists(launchPath)) {
    let parsed;
    try {
      const current = await fsp.readFile(launchPath, "utf8");
      parsed = JSON.parse(current);
    } catch (error) {
      vscode.window.showErrorMessage(`Could not update launch.json: ${error.message}`);
      return;
    }

    if (!Array.isArray(parsed.configurations)) {
      parsed.configurations = [];
    }

    const alreadyExists = parsed.configurations.some((config) => config && config.type === "neo-script");
    if (alreadyExists) {
      vscode.window.showInformationMessage("Neo Script launch config already exists.");
      await vscode.window.showTextDocument(vscode.Uri.file(launchPath));
      return;
    }

    parsed.configurations.push(template);
    launch = parsed;
  }

  await fsp.writeFile(launchPath, `${JSON.stringify(launch, null, 2)}\n`, "utf8");
  await vscode.window.showTextDocument(vscode.Uri.file(launchPath));
  vscode.window.showInformationMessage("Neo Script launch config was created.");
}

class NeoScriptDebugConfigurationProvider {
  constructor(context) {
    this.context = context;
  }

  resolveDebugConfiguration(folder, config) {
    if (!config.type && !config.request && !config.name) {
      const editor = vscode.window.activeTextEditor;
      config.type = "neo-script";
      config.request = "launch";
      config.name = "Debug Neo Script";
      config.program = editor && editor.document.languageId === "neoscript"
        ? editor.document.uri.fsPath
        : "${file}";
      config.cwd = "${workspaceFolder}";
    }

    config.program = resolveVariables(config.program, folder);
    config.cwd = resolveVariables(config.cwd || "${workspaceFolder}", folder);
    config.adapterPath = findAdapterPath(this.context, folder, config.adapterPath);

    if (!config.program) {
      vscode.window.showErrorMessage("Neo Script debug launch needs a program path.");
      return undefined;
    }

    if (!fs.existsSync(config.program)) {
      vscode.window.showErrorMessage(`Neo Script file not found: ${config.program}`);
      return undefined;
    }

    if (!config.adapterPath || !fs.existsSync(config.adapterPath)) {
      vscode.window.showErrorMessage(
        "Neo Script debug adapter was not found. Build Samples/console x64 Release or set neoScript.debugAdapterPath."
      );
      return undefined;
    }

    return config;
  }
}

class NeoScriptDebugAdapterDescriptorFactory {
  constructor(context) {
    this.context = context;
  }

  createDebugAdapterDescriptor(session) {
    const folder = session.workspaceFolder;
    const adapterPath = findAdapterPath(this.context, folder, session.configuration.adapterPath);
    const cwd = resolveVariables(session.configuration.cwd || "${workspaceFolder}", folder) || path.dirname(adapterPath);
    return new vscode.DebugAdapterExecutable(adapterPath, ["--dap"], { cwd });
  }
}

function activate(context) {
  context.subscriptions.push(
    vscode.debug.registerDebugConfigurationProvider(
      "neo-script",
      new NeoScriptDebugConfigurationProvider(context)
    )
  );
  context.subscriptions.push(
    vscode.debug.registerDebugAdapterDescriptorFactory(
      "neo-script",
      new NeoScriptDebugAdapterDescriptorFactory(context)
    )
  );
  context.subscriptions.push(
    vscode.commands.registerCommand("neoScript.createLaunchConfig", createLaunchConfig)
  );
  context.subscriptions.push(
    vscode.commands.registerCommand("neoScript.openSettings", () => {
      vscode.commands.executeCommand("workbench.action.openSettings", "neoScript.debugAdapterPath");
    })
  );
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
