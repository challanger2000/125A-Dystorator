#!/usr/bin/env python3
"""Fail-closed source/package identity gate; binary identity is checked separately by the VST3 host/125A Tester.
No files are changed. Historic upstream names in legal notices are explicitly preserved.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
problems = []

def content(path):
    file = root / path
    if not file.is_file():
        problems.append("MISSING: " + path)
        return ""
    return file.read_text(encoding="utf-8-sig")

def require(path, snippets):
    value = content(path)
    for snippet in snippets:
        if snippet not in value:
            problems.append(f"{path}: required marker absent: {snippet!r}")

require("CMakeLists.txt", [
    "project(Dystorator VERSION 0.1.0)", "juce_add_plugin(Dystorator",
    'COMPANY_NAME "125A Systems"', "PLUGIN_MANUFACTURER_CODE A125",
    "PLUGIN_CODE Dyst", 'PRODUCT_NAME "125A Dystorator"',
])
require("Source/PresetManager.h", [
    '.getChildFile ("125A Systems").getChildFile ("Dystorator").getChildFile ("Presets")',
    '"DystoratorPreset"',
])
require("Source/PluginEditor.cpp", ['g.drawText ("DYSTORATOR"'])
require("tests/ir_presets/CMakeLists.txt", [
    'JucePlugin_Name="125A\\ Dystorator"', 'JucePlugin_VersionString="0.1.0"',
])
require("installer/125A-Dystorator.iss", [
    '#define AppName "125A Dystorator"',
    "AppPublisher=125A Systems",
    "125A Dystorator.vst3",
    "125A-Dystorator-Setup-",
])
require("installer/make_package.ps1", [
    "125A-Dystorator.iss", "125A-Dystorator-$version-Windows",
    "125A-Dystorator-source-$version.zip",
])
require("installer/README.txt", ["125A DYSTORATOR", "AGPLv3"])
require("README.md", ["# 125A Dystorator", "GNU AGPLv3", "SteveAsAConstant/djenty-donkey"])
require("LICENSE", ["GNU AFFERO GENERAL PUBLIC LICENSE"])
require("installer/THIRD-PARTY-NOTICES.txt", ["JUCE", "Steinberg"])

# Do not alter the original open-source authorship or suppress license attribution.
# Old product labels may remain ONLY in attribution notices / historical docs;
# never in current plugin and packaging identifiers.
ship = [
    "CMakeLists.txt", "build.bat", "Source/PresetManager.h", "Source/StandaloneApp.cpp",
    "Source/PluginEditor.cpp", "Source/PluginProcessor.cpp", "Source/PluginProcessor.h",
    "installer/125A-Dystorator.iss", "installer/make_package.ps1", "installer/README.txt",
    "tests/ir_presets/CMakeLists.txt",
]
for path in ship:
    value = content(path).lower()
    for token in ("djenty donkey", "djentydonkey", "donkey head", "vibeamp515_artefacts", "vibeamp 515"):
        if token in value:
            problems.append(f"{path}: legacy shipped product label: {token}")

if (root / "installer" / "DjentyDonkey.iss").exists():
    problems.append("Stale original installer could accidentally be distributed")
if (root / "installer" / "README_CN.txt").exists():
    problems.append("Superseded outdated readme could accidentally be distributed")

if problems:
    for p in problems:
        print("FAIL:", p)
    sys.exit(1)
print("PASS: 125A Dystorator source identity, installer paths, legal materials, and preset namespace")
print("LIMITATION: the compiled VST3 factory/UID and actual Windows install require separate binary-level checks")
