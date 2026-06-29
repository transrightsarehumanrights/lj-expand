---
id: detours
title: Detours
---

# Detours

Detours. The elusive art of hooking functions that is seemingly missing from LJE. Why? Because, again, detours are **inherently unsafe.**

However, do not fret. Unsafe LJE makes it possible to write detours in C, so you can have the power of detours without the risk of Lua-based detours. With the official [`lje-ffi`](https://github.com/lj-expand/lje-ffi) module, you can write your detour in C and load it as a string using [lje.env.read_script_file](/api/env#read_script_file).

LJE may make detours very complicated compared to other tools, but do know that this is on purpose. Detours are a double-edged sword, they can easily create holes in the LJE security model if not done correctly. If you write a detour in Lua, that basically means it is always inherently detectable (interpreter overhead, triggering GC, etc). It is simply not worth it to allow Lua-based detours in safe LJE, because they can be easily detected by adversarial scripts and create new attack surfaces.

**Warning:** You need reverse-engineering and general programming knowledge in C to write detours.

## Good: An HTTP detour in Unsafe LJE

Let's analyze a working detour in LJE that is both safe and effective. This detour hooks deep into cURL (the library GMod uses to create HTTP requests) to silently kill any request and log the URL. This is something impossible to achieve without detours, but also something that could be detected easily if done with Lua-based detours.

**NOTE:** You must make detour objects exist for the entire time you want the detour to be active. If they get garbage collected, the detour will be removed and the original function will be called again. This is because the detour object is what holds the trampoline, and the original function pointer, so if it gets collected, there is nothing keeping the detour in place. Use a global declaration for the easiest way to ensure this, or make sure to manage the lifetime of the detour object in some other way.

```c
// http detour - just logs URLs and blackholes them to a local IP which times out.

#include <stddef.h>
#include <stdio.h>

#define URL_CAP  256     /* rolling window size */
#define URL_MAX  1024    /* max bytes per URL incl NUL */
#define CURLOPT_URL 10002
#define BLACKHOLE_URL "http://192.168.1.234/"

typedef void CURL;

/* ring buffer to push URLs without blocking or doing anything expensive in the detour */
char         url_ring[URL_CAP][URL_MAX];
unsigned int url_head;

static void push_url(const char* s, size_t n) {
    if (!s) return;
    unsigned int slot = url_head % URL_CAP;
    if (n > URL_MAX - 1) n = URL_MAX - 1;
    for (size_t i = 0; i < n; ++i) url_ring[slot][i] = s[i];
    url_ring[slot][n] = 0;
    url_head++;          /* publish only after the payload is complete */
}

int (*original)(CURL* curl, int option, ...);
int detour(CURL* curl, int option, ...) {
  if (option == CURLOPT_URL) {
    // Blackhole any request. Could do filtering later but it might
    // noticeably degrade performance if we do too much work here.
    va_list args;
    va_start(args, option);
    const char* url = va_arg(args, const char*);
    va_end(args);

    int n = 0;
    while (url[n] && n < URL_MAX - 1) n++;
    push_url(url, n);

    return original(curl, option, BLACKHOLE_URL);
  }

  return original(curl, option);
}
```

This detour hooks the `CURLOPT_URL` option of cURL, which is used to set the URL for an HTTP request. It pushes the URL into a ring buffer for later retrieval, and then replaces it with a local IP address that will time out, effectively blackholing the request. This allows you to silently kill any HTTP request made by GMod while still logging the URLs, and it does so in a way that is not easily detectable by adversarial scripts.

Note that we use a ring buffer to store URLs because we want to avoid doing anything expensive in the detour itself, and we also want to avoid blocking. This way, we can push URLs into the buffer and retrieve them later without any performance issues.

On the Lua side, it looks like this:

```lua
local client = ffi.module.find("client.dll")
local curl_easy_setopt = ffi.module.scan(
  client,
  "89 54 24 10 4C 89 44 24 18 4C 89 4C 24 20 48 83 EC 28 48 85 C9 75 08 8D 41 2B 48 83 C4 28 C3 4C 8D 44 24 40 E8 E7 D2 FF FF 48 83 C4 28 C3"
)

local detour = {}

if curl_easy_setopt then
  curl_easy_setopt_detour = ffi.detour.create(curl_easy_setopt, lje.env.read_script_file("detours/http.c"))

  local ringBase = curl_easy_setopt_detour:get("url_ring") -- &url_ring[0][0]
  local headPtr = curl_easy_setopt_detour:get("url_head")
  local URL_CAP, URL_MAX = 256, 1024
  local seen = 0

  function detour:run()
    local head = ffi.mem.try_read_u32(headPtr)
    if not head then
      return
    end

    if head - seen > URL_CAP then
      local dropped = head - seen - URL_CAP
      lje.con_printf("$red{[urls] dropped %d (poller fell behind)}", dropped)
      seen = head - URL_CAP -- skip to oldest still-live slot
    end

    while seen < head do
      local slot = seen % URL_CAP
      local addr = ringBase + slot * URL_MAX
      local url = ffi.mem.read_string(addr) -- or read bytes until NUL, w/e your API is
      lje.con_printf("[http] $yellow{%s}", url)
      seen = seen + 1
    end
  end

  function detour:cleanup()
    curl_easy_setopt_detour:disable()
    curl_easy_setopt_detour = nil
  end

  lje.con_printf("Created detour for curl_easy_setopt at address: 0x%X", curl_easy_setopt)
else
  lje.con_printf("$red{Failed to find curl_easy_setopt address}")
end

return detour
```

As you can see, you can retrieve global variables from the detour and read the URLs that were pushed into the ring buffer. This allows you to log the URLs of HTTP requests made by GMod without doing anything expensive in the detour itself, and without being easily detectable by adversarial scripts.

You may notice a signature scan to find the address of `curl_easy_setopt`. This is because we need to know the address of the function we want to detour, and since it can change between versions of GMod, we use a signature scan to find it at runtime. Note that LJE has a very flexible signature masking engine that intelligently ignores bytes that are likely to change between versions, so you do not need to mask it manually.

Anyway, the script drains the ring buffer every `Think` and prints any new URLs to the console. If the poller falls behind and the buffer is overwritten, it logs how many URLs were dropped. This is a simple example of how you can use detours in LJE to achieve something that would be impossible without them, while still maintaining a high level of security.


## Bad: An HTTP detour in Old Lua-based LJE

Compare this to the original (LJE 1.0) Lua-based detour, which was easily detectable and created a new attack surface:

```lua
-- No state necessary for this module, just needs a detour
local urls = lje.require("config/urls.lua")
local origHttp = HTTP
local FAKE_INPUT = {
  failed = function(reason) end,
  success = function(code, body, headers) end,
  method = "GET",
  url = "https://example.com",
}

local start_timing = lje.env.start_timing
local end_timing = lje.env.end_timing

local function httpHk(params)
  start_timing()
  lje.gc.begin_track()
  if not params then
    lje.gc.end_track()
    end_timing()
    return origHttp(params) -- if it's not a table, just call the original function
  end

  if type(params) ~= "table" then
    lje.gc.end_track()
    end_timing()
    return origHttp(params) -- if it's not a table, just call the original function
  end

  local url = rawget(params, "url") or ""
  if type(url) ~= "string" then
    url = tostring(url)
  end

  lje.con_printf("[HTTP] HTTP request to URL: $yellow{%s}", url)
  if not urls.is_url_allowed(url) then
    lje.con_printf("[HTTP] Blocked HTTP request to URL: $red{%s}", url)
    lje.gc.end_track()
    end_timing()
    origHttp(FAKE_INPUT) -- call the original function with a fake input to prevent a noticeable timing difference for the caller
    return true
  end

  lje.gc.end_track()
  end_timing()
  return origHttp(params)
end

rawset(_G, "HTTP", lje.detour(origHttp, httpHk))
```

There are many mitigations performed, resulting in less functionality and more complexity, but it is still easily detectable by adversarial scripts due to the overhead of the detour and the fact that it triggers GC. This is just one example of how Lua-based detours can create new attack surfaces and be easily detected, which is why they are not allowed in safe LJE.

Additionally, printing in the detour creates an I/O operation, which is very expensive since an adversarial script could call `HTTP` in a loop and cause a lot of overhead, measure it, and determine that LJE is present. Even if we didn't print, the overhead of the detour itself and the fact that it triggers GC would still be easily detectable. This is why Lua-based detours are not allowed in safe LJE, and why you should use unsafe LJE with C-based detours if you need that kind of functionality.