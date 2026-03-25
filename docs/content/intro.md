---
id: intro
title: Introduction
slug: /
---

# lj-expand

lj-expand (LJE) is a stealthy code execution tool for 64-bit Garry's Mod. Simply put, it forcibly installs a stealth layer into the game's
LuaJIT VM which enables you to execute arbitrary Lua code without any of the usual hooks, detections, or mitigations.

LJE is designed to be as undetectable as possible, making it ideal for use in environments where anti-cheat measures are in place.
Of course, with great power comes great responsibility. You are responsible for using LJE and if you choose to use it for malicious purposes, that's on you.

That being said, most bans will come from a handmade script creating a hole in the security model, not from LJE itself. It is therefore important to understand
these APIs and how to use them correctly to avoid detection. LJE is a powerful tool, but it is not a magic bullet. It requires careful use and understanding of the underlying mechanics to be effective.

Head to [Installation](/installation) to get started with LJE!

## API Structure

The LJE API is exposed as a global `lje` table with the following namespaces:

| Namespace | Description |
|---|---|
| `lje` | Global functions (`include`, `con_print`) |
| `lje.func` | Function spoofing, inspection, and stealth |
| `lje.hooks` | Debug hook control |
| `lje.env` | Environment management and global state |
| `lje.util` | Bytecode hashing, stack inspection, utilities |
| `lje.gc` | Garbage collector manipulation |
| `lje.vm` | VM-level bytecode patching and engine hooks |
| `lje.data` | Simple persistent blob storage |
