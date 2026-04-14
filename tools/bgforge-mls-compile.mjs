#!/usr/bin/env node

import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawn } from "node:child_process";

const DEFAULT_COMPILE_OPTIONS = "-q -p -l -O2 -d -s -n";
const EXTENSIONS_DIR = path.join(os.homedir(), ".vscode", "extensions");
const SETTINGS_PATH = path.join(os.homedir(), "Library", "Application Support", "Code", "User", "settings.json");
const EXTENSION_PREFIX = "bgforge.bgforge-mls-";

function usage() {
    console.error("Usage: node tools/bgforge-mls-compile.mjs <input.ssl> [output.int]");
}

function fail(message) {
    console.error(message);
    process.exit(1);
}

function loadJson(filePath) {
    try {
        return JSON.parse(fs.readFileSync(filePath, "utf8"));
    } catch (error) {
        fail(`Failed to read ${filePath}: ${error instanceof Error ? error.message : String(error)}`);
    }
}

function compareVersions(a, b) {
    const aParts = a.split(".").map((part) => Number.parseInt(part, 10) || 0);
    const bParts = b.split(".").map((part) => Number.parseInt(part, 10) || 0);
    const maxLength = Math.max(aParts.length, bParts.length);
    for (let index = 0; index < maxLength; index += 1) {
        const left = aParts[index] ?? 0;
        const right = bParts[index] ?? 0;
        if (left !== right) {
            return left - right;
        }
    }
    return 0;
}

function findExtensionDir() {
    const configured = process.env.BGFORGE_MLS_EXTENSION_DIR;
    if (configured) {
        return configured;
    }

    let entries;
    try {
        entries = fs.readdirSync(EXTENSIONS_DIR, { withFileTypes: true });
    } catch (error) {
        fail(`Failed to read ${EXTENSIONS_DIR}: ${error instanceof Error ? error.message : String(error)}`);
    }

    const candidates = entries
        .filter((entry) => entry.isDirectory() && entry.name.startsWith(EXTENSION_PREFIX))
        .map((entry) => entry.name)
        .sort((left, right) => compareVersions(right.slice(EXTENSION_PREFIX.length), left.slice(EXTENSION_PREFIX.length)));

    if (candidates.length === 0) {
        fail(`Could not find a BGforge MLS extension under ${EXTENSIONS_DIR}`);
    }

    return path.join(EXTENSIONS_DIR, candidates[0]);
}

function loadBgforgeSettings() {
    if (!fs.existsSync(SETTINGS_PATH)) {
        return {};
    }

    const settings = loadJson(SETTINGS_PATH);
    return {
        compileOptions: settings["bgforge.falloutSSL.compileOptions"] || DEFAULT_COMPILE_OPTIONS,
        headersDirectory: settings["bgforge.falloutSSL.headersDirectory"] || "",
    };
}

function splitArgs(text) {
    return text
        .split(/\s+/)
        .map((item) => item.trim())
        .filter((item) => item.length > 0);
}

const [, , inputArg, outputArg] = process.argv;
if (!inputArg) {
    usage();
    process.exit(1);
}

const inputPath = path.resolve(inputArg);
if (!fs.existsSync(inputPath)) {
    fail(`Input file not found: ${inputPath}`);
}

if (path.extname(inputPath).toLowerCase() !== ".ssl") {
    fail(`Expected a .ssl file, got: ${inputPath}`);
}

const outputPath = outputArg
    ? path.resolve(outputArg)
    : path.join(path.dirname(inputPath), `${path.basename(inputPath, ".ssl")}.int`);
const extensionDir = findExtensionDir();
const compilerPath = path.join(extensionDir, "server", "node_modules", "sslc-emscripten-noderawfs", "compiler.mjs");

if (!fs.existsSync(compilerPath)) {
    fail(`BGforge MLS compiler not found: ${compilerPath}`);
}

const settings = loadBgforgeSettings();
if (!settings.headersDirectory) {
    fail(`Missing bgforge.falloutSSL.headersDirectory in ${SETTINGS_PATH}`);
}

const args = [
    compilerPath,
    ...splitArgs(settings.compileOptions),
    `-I${settings.headersDirectory}`,
    path.basename(inputPath),
    "-o",
    outputPath,
];

const child = spawn(process.execPath, args, {
    cwd: path.dirname(inputPath),
    stdio: "inherit",
});

child.on("exit", (code, signal) => {
    if (signal) {
        process.kill(process.pid, signal);
        return;
    }
    process.exit(code ?? 1);
});

child.on("error", (error) => {
    fail(`Failed to start BGforge MLS compiler: ${error instanceof Error ? error.message : String(error)}`);
});
