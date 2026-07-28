#define _GNU_SOURCE

#include "lj_expand_platform.h"
#if !LJE_PLATFORM_WINDOWS

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <link.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "lj_expand_log.h"

#define LJE_PLAT_TODO(what) LJE_ERROR("lje_plat: %s is not implemented on this platform", what)

static const char* path_leaf(const char* p)
{
  const char* s = strrchr(p, '/');
  return s ? s + 1 : p;
}

/* ===== §init ===== */

static void* self_handle = NULL;
static uintptr_t self_base = 0;
static size_t self_size = 0;
static size_t page_sz = 0;
static int probe_fd[2] = { -1, -1 };

typedef struct {
  const char* want_name;
  const void* want_base;
  char found_path[LJE_PATH_MAX];
  uintptr_t base;
  size_t size;
  int found;
} phdr_query;

static int phdr_cb(struct dl_phdr_info* info, size_t sz, void* ud)
{
  (void)sz;
  phdr_query* q = (phdr_query*)ud;
  const char* name = info->dlpi_name ? info->dlpi_name : "";

  if (q->want_name) {
    if (!*name || strcmp(path_leaf(name), q->want_name) != 0)
      return 0;
  } else if ((const void*)info->dlpi_addr != q->want_base) {
    return 0;
  }

  uintptr_t lo = (uintptr_t)-1, hi = 0;
  for (int i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
    if (ph->p_type != PT_LOAD)
      continue;
    uintptr_t s = (uintptr_t)info->dlpi_addr + ph->p_vaddr;
    uintptr_t e = s + ph->p_memsz;
    if (s < lo) lo = s;
    if (e > hi) hi = e;
  }
  if (lo == (uintptr_t)-1)
    return 0;

  q->base = lo;
  q->size = (size_t)(hi - lo);
  lje_strlcpy(q->found_path, name, sizeof(q->found_path));
  q->found = 1;
  return 1;
}

void lje_plat_init(void* native_self_handle)
{
  long ps = sysconf(_SC_PAGESIZE);
  page_sz = (ps > 0) ? (size_t)ps : 4096;

  self_handle = native_self_handle;

  Dl_info di;
  if (dladdr((void*)(uintptr_t)&lje_plat_init, &di) && di.dli_fbase) {
    phdr_query q;
    memset(&q, 0, sizeof(q));
    q.want_base = di.dli_fbase;
    dl_iterate_phdr(phdr_cb, &q);
    if (q.found) {
      self_base = q.base;
      self_size = q.size;
    }
    if (!self_handle && di.dli_fname && *di.dli_fname)
      self_handle = dlopen(di.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
  }

  if (pipe2(probe_fd, O_NONBLOCK | O_CLOEXEC) != 0) {
    probe_fd[0] = probe_fd[1] = -1;
  }
}

/* ===== §modules ===== */

static char module_search_path[LJE_PATH_MAX] = { 0 };

int lje_plat_module_find(const char* name, LJEPlatModule* out)
{
  phdr_query q;
  memset(&q, 0, sizeof(q));
  q.want_name = path_leaf(name);
  dl_iterate_phdr(phdr_cb, &q);
  if (!q.found)
    return 0;

  out->base = q.base;
  out->size = q.size;
  out->handle = dlopen(q.found_path, RTLD_LAZY | RTLD_NOLOAD);
  return 1;
}

void* lje_plat_module_sym(const LJEPlatModule* m, const char* sym)
{
  if (!m->handle)
    return dlsym(RTLD_DEFAULT, sym);
  return dlsym(m->handle, sym);
}

void* lje_plat_module_load(const char* path)
{
  if (path[0] != '/' && module_search_path[0]) {
    char joined[LJE_PATH_MAX];
    if (lje_path_join(joined, sizeof(joined), module_search_path, path)) {
      void* h = dlopen(joined, RTLD_NOW | RTLD_LOCAL);
      if (h)
        return h;
    }
  }
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* lje_plat_module_sym_raw(void* handle, const char* sym)
{
  return dlsym(handle, sym);
}

void lje_plat_module_unload(void* handle)
{
  if (handle)
    dlclose(handle);
}

/* No POSIX equivalent of SetDllDirectory: glibc fixes the DT_NEEDED search path
 * at startup. This only redirects relative paths handed to module_load; binary
 * modules that need siblings resolved must be linked -Wl,-rpath,'$ORIGIN'. */
void lje_plat_module_search_dir(const char* dir)
{
  if (dir)
    lje_strlcpy(module_search_path, dir, sizeof(module_search_path));
  else
    module_search_path[0] = '\0';
}

int lje_plat_module_name_from_addr(const void* addr, char* out, size_t n)
{
  Dl_info di;
  if (!dladdr(addr, &di) || !di.dli_fname || !*di.dli_fname)
    return 0;
  lje_strlcpy(out, path_leaf(di.dli_fname), n);
  return 1;
}

int lje_plat_self_range(uintptr_t* base, size_t* size)
{
  if (!self_base || !self_size)
    return 0;
  *base = self_base;
  *size = self_size;
  return 1;
}

/* ===== §memory ===== */

size_t lje_plat_page_size(void)
{
  return page_sz;
}

int lje_plat_protect(void* addr, size_t len, int prot)
{
  uintptr_t start = (uintptr_t)addr & ~(uintptr_t)(page_sz - 1);
  size_t aligned_len = ((uintptr_t)addr + len - start + page_sz - 1) & ~(uintptr_t)(page_sz - 1);

  int flags;
  switch (prot) {
  case LJE_PROT_RX:  flags = PROT_READ | PROT_EXEC;              break;
  case LJE_PROT_RW:  flags = PROT_READ | PROT_WRITE;             break;
  case LJE_PROT_RWX: flags = PROT_READ | PROT_WRITE | PROT_EXEC; break;
  default: return 0;
  }

  return mprotect((void*)start, aligned_len, flags) == 0 ? 1 : 0;
}

void lje_plat_flush_icache(void* addr, size_t len)
{
  __builtin___clear_cache((char*)addr, (char*)addr + len);
}

/* Probes readability by handing the address to the kernel: write() reports
 * EFAULT rather than faulting, so this stays safe inside a signal handler. */
int lje_plat_addr_readable(const void* addr)
{
  if (!addr)
    return 0;

  if (probe_fd[1] >= 0) {
    ssize_t w = write(probe_fd[1], addr, 1);
    if (w == 1) {
      char sink;
      ssize_t r = read(probe_fd[0], &sink, 1);
      (void)r;
      return 1;
    }
    if (w < 0 && errno == EFAULT)
      return 0;
  }

  uintptr_t p = (uintptr_t)addr & ~(uintptr_t)(page_sz - 1);
  return msync((void*)p, page_sz, MS_ASYNC) == 0 ? 1 : 0;
}

/* ===== §fs ===== */

int lje_plat_fs_kind(const char* path)
{
  struct stat st;
  if (stat(path, &st) != 0)
    return LJE_FS_MISSING;
  return S_ISDIR(st.st_mode) ? LJE_FS_DIR : LJE_FS_FILE;
}

int lje_plat_mkdir(const char* path)
{
  if (mkdir(path, 0755) == 0)
    return 1;
  return errno == EEXIST ? 1 : 0;
}

int lje_plat_mkdirs(const char* path)
{
  char tmp[LJE_PATH_MAX];
  if (lje_strlcpy(tmp, path, sizeof(tmp)) >= sizeof(tmp))
    return 0;

  size_t len = strlen(tmp);
  while (len > 1 && tmp[len - 1] == '/')
    tmp[--len] = '\0';

  for (char* p = tmp + 1; *p; p++) {
    if (*p != '/')
      continue;
    *p = '\0';
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
      return 0;
    *p = '/';
  }

  if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    return 0;
  return 1;
}

int lje_plat_copy_file(const char* src, const char* dst)
{
  int in = open(src, O_RDONLY | O_CLOEXEC);
  if (in < 0)
    return 0;

  struct stat st;
  mode_t mode = (fstat(in, &st) == 0) ? (st.st_mode & 0777) : 0644;

  int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
  if (out < 0) {
    close(in);
    return 0;
  }

  char buf[8192];
  int ok = 1;
  for (;;) {
    ssize_t got = read(in, buf, sizeof(buf));
    if (got == 0)
      break;
    if (got < 0) {
      if (errno == EINTR) continue;
      ok = 0;
      break;
    }
    ssize_t off = 0;
    while (off < got) {
      ssize_t put = write(out, buf + off, (size_t)(got - off));
      if (put < 0) {
        if (errno == EINTR) continue;
        ok = 0;
        break;
      }
      off += put;
    }
    if (!ok)
      break;
  }

  close(in);
  close(out);
  return ok;
}

int lje_plat_home_dir(char* out, size_t n)
{
  const char* home = getenv("HOME");
  if (!home || !*home) {
    struct passwd* pw = getpwuid(getuid());
    home = (pw && pw->pw_dir) ? pw->pw_dir : NULL;
  }
  if (!home || !*home)
    return 0;
  if (lje_strlcpy(out, home, n) >= n)
    return 0;

  size_t len = strlen(out);
  while (len > 1 && out[len - 1] == '/')
    out[--len] = '\0';
  return 1;
}

int lje_plat_expand_env(const char* in, char* out, size_t n)
{
  size_t o = 0;
  const char* p = in;

  while (*p) {
    const char* name = NULL;
    const char* after = NULL;
    size_t name_len = 0;

    if (*p == '$' && p[1] == '{') {
      const char* end = strchr(p + 2, '}');
      if (end) { name = p + 2; name_len = (size_t)(end - name); after = end + 1; }
    } else if (*p == '$' && (isalpha((unsigned char)p[1]) || p[1] == '_')) {
      const char* q = p + 1;
      while (*q && (isalnum((unsigned char)*q) || *q == '_')) q++;
      name = p + 1; name_len = (size_t)(q - name); after = q;
    } else if (*p == '%') {
      const char* end = strchr(p + 1, '%');
      if (end && end > p + 1) { name = p + 1; name_len = (size_t)(end - name); after = end + 1; }
    }

    if (name) {
      char key[128];
      const char* val = NULL;
      if (name_len < sizeof(key)) {
        memcpy(key, name, name_len);
        key[name_len] = '\0';
        val = getenv(key);
      }
      const char* text = val ? val : p;   /* unresolved names pass through verbatim */
      size_t text_len = val ? strlen(val) : (size_t)(after - p);
      if (o + text_len >= n)
        return 0;
      memcpy(out + o, text, text_len);
      o += text_len;
      p = after;
      continue;
    }

    if (o + 1 >= n)
      return 0;
    out[o++] = *p++;
  }

  out[o] = '\0';
  return 1;
}

typedef struct {
  uint32_t h[8];
  uint64_t bits;
  uint8_t buf[64];
  size_t n;
} sha256_ctx;

static const uint32_t sha256_k[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
  0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
  0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
  0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
  0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
  0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define SHA_ROR(x, k) (((x) >> (k)) | ((x) << (32 - (k))))

static void sha256_block(sha256_ctx* c, const uint8_t* p)
{
  uint32_t w[64];
  for (int i = 0; i < 16; i++)
    w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
           ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = SHA_ROR(w[i - 15], 7) ^ SHA_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = SHA_ROR(w[i - 2], 17) ^ SHA_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
  uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];

  for (int i = 0; i < 64; i++) {
    uint32_t s1 = SHA_ROR(e, 6) ^ SHA_ROR(e, 11) ^ SHA_ROR(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = h + s1 + ch + sha256_k[i] + w[i];
    uint32_t s0 = SHA_ROR(a, 2) ^ SHA_ROR(a, 13) ^ SHA_ROR(a, 22);
    uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
    uint32_t t2 = s0 + maj;
    h = g; g = f; f = e; e = d + t1;
    d = cc; cc = b; b = a; a = t1 + t2;
  }

  c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
  c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void sha256_init(sha256_ctx* c)
{
  c->h[0] = 0x6a09e667; c->h[1] = 0xbb67ae85;
  c->h[2] = 0x3c6ef372; c->h[3] = 0xa54ff53a;
  c->h[4] = 0x510e527f; c->h[5] = 0x9b05688c;
  c->h[6] = 0x1f83d9ab; c->h[7] = 0x5be0cd19;
  c->bits = 0;
  c->n = 0;
}

static void sha256_update(sha256_ctx* c, const uint8_t* p, size_t len)
{
  c->bits += (uint64_t)len * 8;
  while (len) {
    size_t take = 64 - c->n;
    if (take > len) take = len;
    memcpy(c->buf + c->n, p, take);
    c->n += take;
    p += take;
    len -= take;
    if (c->n == 64) {
      sha256_block(c, c->buf);
      c->n = 0;
    }
  }
}

static void sha256_final(sha256_ctx* c, uint8_t out[32])
{
  uint64_t bits = c->bits;
  uint8_t pad = 0x80;
  sha256_update(c, &pad, 1);
  c->bits = bits;
  uint8_t zero = 0;
  while (c->n != 56) {
    sha256_update(c, &zero, 1);
    c->bits = bits;
  }
  uint8_t len_be[8];
  for (int i = 0; i < 8; i++)
    len_be[i] = (uint8_t)(bits >> (56 - i * 8));
  sha256_update(c, len_be, 8);

  for (int i = 0; i < 8; i++) {
    out[i * 4]     = (uint8_t)(c->h[i] >> 24);
    out[i * 4 + 1] = (uint8_t)(c->h[i] >> 16);
    out[i * 4 + 2] = (uint8_t)(c->h[i] >> 8);
    out[i * 4 + 3] = (uint8_t)(c->h[i]);
  }
}

int lje_plat_sha256_file(const char* path, char out_hex[65])
{
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return 0;

  sha256_ctx c;
  sha256_init(&c);

  uint8_t buf[4096];
  int ok = 1;
  for (;;) {
    ssize_t got = read(fd, buf, sizeof(buf));
    if (got == 0)
      break;
    if (got < 0) {
      if (errno == EINTR) continue;
      ok = 0;
      break;
    }
    sha256_update(&c, buf, (size_t)got);
  }
  close(fd);

  if (!ok)
    return 0;

  uint8_t digest[32];
  sha256_final(&c, digest);
  for (int i = 0; i < 32; i++)
    snprintf(&out_hex[i * 2], 3, "%02x", digest[i]);
  out_hex[64] = '\0';
  return 1;
}

struct LJEPlatDir {
  DIR* d;
  char path[LJE_PATH_MAX];
  char pattern[LJE_NAME_MAX];
  int has_pattern;
};

LJEPlatDir* lje_plat_dir_open(const char* dir, const char* pattern)
{
  DIR* handle = opendir(dir);
  if (!handle)
    return NULL;

  LJEPlatDir* d = (LJEPlatDir*)calloc(1, sizeof(LJEPlatDir));
  if (!d) {
    closedir(handle);
    return NULL;
  }

  d->d = handle;
  lje_strlcpy(d->path, dir, sizeof(d->path));
  if (pattern && strcmp(pattern, "*") != 0) {
    lje_strlcpy(d->pattern, pattern, sizeof(d->pattern));
    d->has_pattern = 1;
  }
  return d;
}

int lje_plat_dir_next(LJEPlatDir* d, LJEPlatDirEntry* out)
{
  struct dirent* e;
  while ((e = readdir(d->d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    if (d->has_pattern && fnmatch(d->pattern, e->d_name, FNM_CASEFOLD) != 0)
      continue;

    lje_strlcpy(out->name, e->d_name, sizeof(out->name));

    if (e->d_type == DT_DIR) {
      out->is_dir = 1;
    } else if (e->d_type == DT_UNKNOWN || e->d_type == DT_LNK) {
      char full[LJE_PATH_MAX];
      struct stat st;
      out->is_dir = (lje_path_join(full, sizeof(full), d->path, e->d_name) &&
                     stat(full, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
    } else {
      out->is_dir = 0;
    }
    return 1;
  }
  return 0;
}

void lje_plat_dir_close(LJEPlatDir* d)
{
  if (d) {
    closedir(d->d);
    free(d);
  }
}

/* ===== §watch ===== */

#define LJE_WATCH_MASK (IN_CREATE | IN_DELETE | IN_MODIFY | IN_CLOSE_WRITE | \
                        IN_MOVED_FROM | IN_MOVED_TO)

typedef struct {
  int wd;
  char* rel;         /* path relative to the watch root; "" for the root itself */
} watch_dir;

struct LJEPlatWatch {
  int fd;
  char root[LJE_PATH_MAX];
  watch_dir* dirs;
  size_t ndirs, cap;
  char buf[8192] __attribute__((aligned(8)));
  ssize_t len;
  ssize_t off;
};

static int watch_track(LJEPlatWatch* w, int wd, const char* rel)
{
  /* The kernel reuses watch descriptors after a directory goes away, so an
   * existing entry must be repointed rather than kept. */
  for (size_t i = 0; i < w->ndirs; i++) {
    if (w->dirs[i].wd != wd)
      continue;
    char* dup = strdup(rel);
    if (!dup)
      return 0;
    free(w->dirs[i].rel);
    w->dirs[i].rel = dup;
    return 1;
  }
  if (w->ndirs == w->cap) {
    size_t cap = w->cap ? w->cap * 2 : 16;
    watch_dir* grown = (watch_dir*)realloc(w->dirs, cap * sizeof(watch_dir));
    if (!grown)
      return 0;
    w->dirs = grown;
    w->cap = cap;
  }
  w->dirs[w->ndirs].wd = wd;
  w->dirs[w->ndirs].rel = strdup(rel);
  if (!w->dirs[w->ndirs].rel)
    return 0;
  w->ndirs++;
  return 1;
}

static const char* watch_rel_of(LJEPlatWatch* w, int wd)
{
  for (size_t i = 0; i < w->ndirs; i++) {
    if (w->dirs[i].wd == wd)
      return w->dirs[i].rel;
  }
  return NULL;
}

/* inotify is not recursive, so every subdirectory needs its own watch. */
static void watch_add_tree(LJEPlatWatch* w, const char* abs, const char* rel)
{
  int wd = inotify_add_watch(w->fd, abs, LJE_WATCH_MASK);
  if (wd < 0 || !watch_track(w, wd, rel))
    return;

  DIR* d = opendir(abs);
  if (!d)
    return;

  struct dirent* e;
  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;

    char child_abs[LJE_PATH_MAX];
    if (!lje_path_join(child_abs, sizeof(child_abs), abs, e->d_name))
      continue;

    if (e->d_type != DT_DIR) {
      if (e->d_type != DT_UNKNOWN && e->d_type != DT_LNK)
        continue;
      struct stat st;
      if (stat(child_abs, &st) != 0 || !S_ISDIR(st.st_mode))
        continue;
    }

    char child_rel[LJE_PATH_MAX];
    if (*rel) {
      if (!lje_path_join(child_rel, sizeof(child_rel), rel, e->d_name))
        continue;
    } else {
      lje_strlcpy(child_rel, e->d_name, sizeof(child_rel));
    }
    watch_add_tree(w, child_abs, child_rel);
  }
  closedir(d);
}

LJEPlatWatch* lje_plat_watch_open(const char* dir)
{
  LJEPlatWatch* w = (LJEPlatWatch*)calloc(1, sizeof(LJEPlatWatch));
  if (!w)
    return NULL;

  w->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (w->fd < 0) {
    free(w);
    return NULL;
  }

  lje_strlcpy(w->root, dir, sizeof(w->root));
  watch_add_tree(w, dir, "");

  if (w->ndirs == 0) {
    close(w->fd);
    free(w);
    return NULL;
  }
  return w;
}

int lje_plat_watch_poll(LJEPlatWatch* w, char* name_out, size_t n)
{
  for (;;) {
    if (w->off >= w->len) {
      ssize_t got = read(w->fd, w->buf, sizeof(w->buf));
      if (got < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
          return LJE_WATCH_NONE;
        return LJE_WATCH_ERROR;
      }
      if (got == 0)
        return LJE_WATCH_NONE;
      w->len = got;
      w->off = 0;
    }

    const struct inotify_event* e = (const struct inotify_event*)(w->buf + w->off);
    w->off += (ssize_t)(sizeof(struct inotify_event) + e->len);

    if (e->mask & IN_Q_OVERFLOW)
      return LJE_WATCH_OVERFLOW;
    if (e->mask & IN_IGNORED)
      continue;
    if (e->len == 0)
      continue;

    if (e->mask & IN_ISDIR) {
      if (e->mask & (IN_CREATE | IN_MOVED_TO)) {
        const char* parent_rel = watch_rel_of(w, e->wd);
        char abs[LJE_PATH_MAX], rel[LJE_PATH_MAX];
        char parent_abs[LJE_PATH_MAX];

        if (parent_rel && *parent_rel) {
          if (!lje_path_join(parent_abs, sizeof(parent_abs), w->root, parent_rel))
            continue;
          if (!lje_path_join(rel, sizeof(rel), parent_rel, e->name))
            continue;
        } else {
          lje_strlcpy(parent_abs, w->root, sizeof(parent_abs));
          lje_strlcpy(rel, e->name, sizeof(rel));
        }
        if (lje_path_join(abs, sizeof(abs), parent_abs, e->name))
          watch_add_tree(w, abs, rel);

        /* Files may have appeared inside before the watch attached; the caller
         * must rescan rather than trust the events it has seen. */
        return LJE_WATCH_OVERFLOW;
      }
      continue;
    }

    const char* rel = watch_rel_of(w, e->wd);
    if (rel && *rel) {
      if (!lje_path_join(name_out, n, rel, e->name))
        lje_strlcpy(name_out, e->name, n);
    } else {
      lje_strlcpy(name_out, e->name, n);
    }
    return LJE_WATCH_EVENT;
  }
}

void lje_plat_watch_close(LJEPlatWatch* w)
{
  if (!w)
    return;
  for (size_t i = 0; i < w->ndirs; i++)
    free(w->dirs[i].rel);
  free(w->dirs);
  close(w->fd);
  free(w);
}

/* ===== §thread ===== */

struct LJEPlatThread {
  pthread_t tid;
  LJEPlatThreadFn fn;
  void* ud;
};

struct LJEPlatMutex {
  pthread_mutex_t m;
};

static void* thread_trampoline(void* arg)
{
  LJEPlatThread* t = (LJEPlatThread*)arg;
  t->fn(t->ud);
  return NULL;
}

LJEPlatThread* lje_plat_thread_start(LJEPlatThreadFn fn, void* ud)
{
  LJEPlatThread* t = (LJEPlatThread*)malloc(sizeof(LJEPlatThread));
  if (!t)
    return NULL;
  t->fn = fn;
  t->ud = ud;
  if (pthread_create(&t->tid, NULL, thread_trampoline, t) != 0) {
    free(t);
    return NULL;
  }
  return t;
}

void lje_plat_thread_join(LJEPlatThread* t)
{
  if (t) {
    pthread_join(t->tid, NULL);
    free(t);
  }
}

/* CRITICAL_SECTION is reentrant, so the mutex must be too or the watcher
 * deadlocks on any nested lock. */
LJEPlatMutex* lje_plat_mutex_create(void)
{
  LJEPlatMutex* m = (LJEPlatMutex*)malloc(sizeof(LJEPlatMutex));
  if (!m)
    return NULL;

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  int rc = pthread_mutex_init(&m->m, &attr);
  pthread_mutexattr_destroy(&attr);

  if (rc != 0) {
    free(m);
    return NULL;
  }
  return m;
}

void lje_plat_mutex_lock(LJEPlatMutex* m)
{
  pthread_mutex_lock(&m->m);
}

void lje_plat_mutex_unlock(LJEPlatMutex* m)
{
  pthread_mutex_unlock(&m->m);
}

void lje_plat_mutex_destroy(LJEPlatMutex* m)
{
  if (m) {
    pthread_mutex_destroy(&m->m);
    free(m);
  }
}

void lje_plat_barrier(void)
{
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void lje_plat_sleep_ms(unsigned ms)
{
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000);
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
    ;
}

uint64_t lje_plat_ticks_ms(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000L);
}

void lje_plat_local_time(LJEPlatTime* out)
{
  time_t now = time(NULL);
  struct tm tmv;
  localtime_r(&now, &tmv);
  out->year   = tmv.tm_year + 1900;
  out->month  = tmv.tm_mon + 1;
  out->day    = tmv.tm_mday;
  out->hour   = tmv.tm_hour;
  out->minute = tmv.tm_min;
  out->second = tmv.tm_sec;
}

/* ===== §console ===== */

void lje_plat_console_init(const char* title)
{
  /* ANSI already works; the real job is stopping a piped stdout from swallowing logs. */
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (title && isatty(STDOUT_FILENO))
    fprintf(stdout, "\033]0;%s\007", title);
}

const char* lje_plat_command_line(void)
{
  static char cmdline[LJE_PATH_MAX * 2];
  static int loaded = 0;

  if (loaded)
    return cmdline;
  loaded = 1;

  int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return cmdline;

  ssize_t got = read(fd, cmdline, sizeof(cmdline) - 1);
  close(fd);
  if (got <= 0)
    return cmdline;

  for (ssize_t i = 0; i < got - 1; i++) {
    if (cmdline[i] == '\0')
      cmdline[i] = ' ';
  }
  cmdline[got] = '\0';
  return cmdline;
}

void lje_plat_message_box(const char* title, const char* text)
{
  fprintf(stderr, "\n*** %s ***\n%s\n\n", title, text);
  fflush(stderr);
}

void lje_plat_debug_break(void)
{
  raise(SIGTRAP);
}

/* ===== §crash ===== */

static LJEPlatCrashFn crash_cb = NULL;
static void* crash_cb_ud = NULL;

static const int crash_signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL };
#define LJE_CRASH_NSIG ((int)(sizeof(crash_signals) / sizeof(crash_signals[0])))

static struct sigaction crash_old[LJE_CRASH_NSIG];
static char crash_stack[64 * 1024];

static const char* signal_name(int sig)
{
  switch (sig) {
  case SIGSEGV: return "SIGSEGV";
  case SIGBUS:  return "SIGBUS";
  case SIGFPE:  return "SIGFPE";
  case SIGILL:  return "SIGILL";
  default:      return "SIGNAL";
  }
}

static void crash_handler(int sig, siginfo_t* si, void* uctx)
{
  LJEPlatCrashInfo info;
  memset(&info, 0, sizeof(info));

  info.native_code = (uint32_t)sig;
  info.name = signal_name(sig);
  info.fault_addr = si ? si->si_addr : NULL;
  info.native = uctx;

  if (uctx) {
    const greg_t* g = ((ucontext_t*)uctx)->uc_mcontext.gregs;
    info.regs[LJE_REG_RAX] = (uint64_t)g[REG_RAX];
    info.regs[LJE_REG_RBX] = (uint64_t)g[REG_RBX];
    info.regs[LJE_REG_RCX] = (uint64_t)g[REG_RCX];
    info.regs[LJE_REG_RDX] = (uint64_t)g[REG_RDX];
    info.regs[LJE_REG_RSI] = (uint64_t)g[REG_RSI];
    info.regs[LJE_REG_RDI] = (uint64_t)g[REG_RDI];
    info.regs[LJE_REG_RBP] = (uint64_t)g[REG_RBP];
    info.regs[LJE_REG_RSP] = (uint64_t)g[REG_RSP];
    info.regs[LJE_REG_R8]  = (uint64_t)g[REG_R8];
    info.regs[LJE_REG_R9]  = (uint64_t)g[REG_R9];
    info.regs[LJE_REG_R10] = (uint64_t)g[REG_R10];
    info.regs[LJE_REG_R11] = (uint64_t)g[REG_R11];
    info.regs[LJE_REG_R12] = (uint64_t)g[REG_R12];
    info.regs[LJE_REG_R13] = (uint64_t)g[REG_R13];
    info.regs[LJE_REG_R14] = (uint64_t)g[REG_R14];
    info.regs[LJE_REG_R15] = (uint64_t)g[REG_R15];
    info.regs[LJE_REG_RIP] = (uint64_t)g[REG_RIP];
  }

  int frames = backtrace(info.frames, LJE_CRASH_MAX_FRAMES);
  info.frame_count = (frames > 0) ? (size_t)frames : 0;

  int handled = crash_cb ? crash_cb(&info, crash_cb_ud) : 0;
  if (handled)
    _exit(1);

  /* Restoring the previous disposition and returning re-runs the faulting
   * instruction, which reproduces the crash for whoever handles it next. */
  for (int i = 0; i < LJE_CRASH_NSIG; i++) {
    if (crash_signals[i] == sig)
      sigaction(sig, &crash_old[i], NULL);
  }
}

int lje_plat_crash_install(LJEPlatCrashFn fn, void* ud)
{
  crash_cb = fn;
  crash_cb_ud = ud;

  stack_t ss;
  ss.ss_sp = crash_stack;
  ss.ss_size = sizeof(crash_stack);
  ss.ss_flags = 0;
  sigaltstack(&ss, NULL);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);

  int ok = 1;
  for (int i = 0; i < LJE_CRASH_NSIG; i++) {
    if (sigaction(crash_signals[i], &sa, &crash_old[i]) != 0)
      ok = 0;
  }
  return ok;
}

int lje_plat_crash_write_native_dump(const LJEPlatCrashInfo* info, const char* path)
{
  (void)info; (void)path;
  return 0;
}

/* ===== §strings ===== */

size_t lje_strlcpy(char* dst, const char* src, size_t n)
{
  if (n == 0) return strlen(src);
  size_t i;
  for (i = 0; i < n - 1 && src[i]; i++)
    dst[i] = src[i];
  dst[i] = '\0';
  while (src[i]) i++;
  return i;
}

size_t lje_strlcat(char* dst, const char* src, size_t n)
{
  size_t dlen = 0;
  while (dlen < n && dst[dlen])
    dlen++;
  if (dlen == n) return dlen + strlen(src);
  size_t i;
  for (i = 0; dlen + i < n - 1 && src[i]; i++)
    dst[dlen + i] = src[i];
  dst[dlen + i] = '\0';
  return dlen + (i + strlen(src + i));
}

int lje_path_join(char* out, size_t n, const char* a, const char* b)
{
  size_t alen = strlen(a);
  size_t blen = strlen(b);
  int need_sep = (alen > 0 && a[alen - 1] != '/' && a[alen - 1] != '\\');
  size_t total = alen + (need_sep ? 1 : 0) + blen;
  if (total >= n) return 0;
  size_t pos = 0;
  size_t i;
  for (i = 0; i < alen; i++)
    out[pos++] = a[i];
  if (need_sep)
    out[pos++] = LJE_PATH_SEP;
  for (i = 0; i < blen; i++)
    out[pos++] = b[i];
  out[pos] = '\0';
  return 1;
}

#endif /* !LJE_PLATFORM_WINDOWS */
