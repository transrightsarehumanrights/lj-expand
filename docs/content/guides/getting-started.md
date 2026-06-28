---
id: getting-started
title: Getting Started
---

## What is LJE?

Cutting through all the slop, LJE is simply a Lua runner at its core. You have GMod-like Lua scripts, and LJE runs them in a secure Lua universe separate from GMod. You **do not** run in the same Lua state as GMod, and you **do not** have access to the same globals as GMod. You have your own Lua state, your own globals, and your own environment. The only thing you share with GMod is the API.

Similarly, detours (in the typical Lua sense) and altering hook behavior is forbidden in safe LJE. This is on purpose, any behavior change driven by an LJE script inherently creates a new attack surface, and LJE is designed to be as secure as possible. If you want to do that kind of thing, you can write [unsafe LJE](#unsafe-lje), but be aware of the risks.

LJE's core purpose is to run Lua code in a secure environment with access to the GMod API, and that's what it does. You can write typical GMod Lua code, but you can't do things that would be unsafe or create new attack surfaces. If you want to though, you are completely free to do so, just be aware that you can and probably will break things or be detected by adversarial scripts if you do.

To extend functionality, LJE supports binary modules just like GMod, but these are securely loaded in for the entire GMod process, so they can be used by any script and at any time. You can write your own binary modules to extend the API in ways that would be impossible in Lua, and these modules are loaded without the GMod Lua state knowing anything about them, so they can't be tampered with or detected by adversarial scripts.

## How does LJE work?

LJE works as follows:
1. The engine creates the main Lua state once a game is starting up.
2. LJE redirects all calls to the Lua API into its own Lua state, which is separate from the main Lua state that GMod uses.
3. LJE gains a **shadow** of the GMod API in its own Lua state.
4. LJE runs scripts in its own Lua state, giving them access to the shadow API but not the main Lua state or its globals.
5. LJE uses Magic™️ (luajit) to allow LJE scripts to call into GMod without any clones or proxies, giving them the illusion of running in the same Lua state as GMod while still being completely separate and secure.
6. LJE also provides proxied engine call hooks to observe, but not alter, engine behavior. This allows you to use hooks like `PostRender` and `Think` without actually being in the same Lua state as GMod, and all proxies are cheap as possible to avoid performance issues.

Essentially, it clones the main GMod API into its own Lua state and then runs scripts in that state, while still allowing them to call into GMod as if they were running in the same state. This gives you the best of both worlds: access to the GMod API without any of the risks of running in the same Lua state as GMod. It also means you can safely expose dangerous binary modules to your scripts without worrying about adversarial scripts detecting them or tampering with them, since they can't even see them.

## Safe LJE

There is something fairly major that sticks out to many people when they first use LJE: **no detours.**

This is on purpose. It is simply an inherent property of Lua detours that they can and will become oracles for adversarial scripts. Oracles are essentially unintentional APIs that adversarial scripts can use to detect the presence of LJE or other scripts, or to detect when certain code is running. Detours are a prime example of this, because they change the behavior of existing functions in a way that adversarial scripts can easily detect. For example, if you detour `hook.Add` to print something every time it's called, an adversarial script could call `hook.Add` over and over and determine that LJE is present because of the additional time it takes for the detour to run. This is just one example, but there are countless ways that detours can be used as oracles, and it's simply not worth it to allow them in safe LJE.

Unsafe LJE allows you to write detours in C so you can have the power of detours without the risk of Lua-based detours, but safe LJE is designed to be as secure as possible, and that means not allowing any behavior that could be used as an oracle for adversarial scripts.

With the current API, you can still achieve a lot without detours. You can still observe hooks and interact with the GMod API, which is typically what most scripts will do. For example, you can still hook `PostRender` and use `surface` or `render` without any problem.

## Unsafe LJE

LJE can be extended to allow unsafe behavior, such as detours and hook alterations. Unsafe LJE is only enabled by the official yet opt-in binary module, `lje-ffi`, which you can install [here](https://github.com/lj-expand/lje-ffi). Unsafe LJE, however, is not inherently less secure, it's just that it allows you to do things that could be used in an unsafe way. If you know what you're doing and are careful, you can use unsafe LJE without creating new attack surfaces, and gain detours and other powerful features in the process. If you don't know what you're doing, however, you can easily break things or create new attack surfaces, so be sure to understand the risks before using unsafe LJE.

This module gives you complete and total access to the entire process by providing a general-purpose but game-oriented FFI library. With it, you can detour anything, load arbitrary DLLs, read/write process memory, and do anything else you can do with a typical FFI library. This is very powerful, but also very dangerous, so be sure you know what you're doing if you choose to use it. 