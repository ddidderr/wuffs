// Copyright 2026 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

/*
brcat decodes (or, with -encode, encodes) brotli from stdin to stdout. It is
similar to "brotli -dc" (or "brotli -c"), with fewer options.

Flags:
  -encode              encode instead of decode.
  -large-window        use large-window brotli (RFC 7932 does not cover it).
  -lgwin=N             the encoder's window size, 10 to 24 (or 30 with
                       -large-window). The default is 22.
  -quality=N           the encoder's quality, 0 to 11. The default is 11.
  -fail-if-unsandboxed exit with an error if the seccomp sandbox failed.

The encoder's output is byte-for-byte identical, on every platform, to what
the brotli reference implementation's library (libbrotlienc v1.2.1) produces
for the same parameters and the same sequence of calls. That output depends
on how the input is split into calls (for example, the first call's length is
the default size hint), so it can differ from the "brotli" command line tool's,
which reads 512 KiB at a time and sets a size hint. Like that reference
implementation, qualities 10 and 11 are much slower than lower qualities.

To run:

$CC brcat.c && ./a.out -encode < ../../test/data/romeo.txt | ./a.out; rm -f a.out

for a C compiler $CC, such as clang or gcc.
*/

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
#define WUFFS_CONFIG__STATIC_FUNCTIONS

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. std/brotli's encoder
// uses std/softfloat.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BROTLI
#define WUFFS_CONFIG__MODULE__SOFTFLOAT

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

#if defined(__linux__)
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#define WUFFS_EXAMPLE_USE_SECCOMP
#endif

#ifndef DST_BUFFER_ARRAY_SIZE
#define DST_BUFFER_ARRAY_SIZE (1024 * 1024)
#endif

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (1024 * 1024)
#endif

uint8_t g_dst_buffer_array[DST_BUFFER_ARRAY_SIZE];
uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];

// The workbuf is allocated (with malloc) before entering the sandbox, as its
// length depends on the flags.
wuffs_base__slice_u8 g_workbuf = {0};

static bool g_sandboxed = false;

wuffs_base__io_transformer* g_io_transformer = NULL;
union {
  wuffs_brotli__decoder dec;
  wuffs_brotli__encoder enc;
} g_codec;

// ----

struct {
  int remaining_argc;
  char** remaining_argv;

  bool encode;
  bool fail_if_unsandboxed;
  bool large_window;
  uint32_t lgwin;
  uint32_t quality;
} g_flags = {0};

const char*  //
parse_flags(int argc, char** argv) {
  g_flags.lgwin = 22;
  g_flags.quality = 11;

  int c = (argc > 0) ? 1 : 0;  // Skip argv[0], the program name.
  for (; c < argc; c++) {
    char* arg = argv[c];
    if (*arg++ != '-') {
      break;
    }

    // A double-dash "--foo" is equivalent to a single-dash "-foo". As special
    // cases, a bare "-" is not a flag (some programs may interpret it as
    // stdin) and a bare "--" means to stop parsing flags.
    if (*arg == '\x00') {
      break;
    } else if (*arg == '-') {
      arg++;
      if (*arg == '\x00') {
        c++;
        break;
      }
    }

    if (!strcmp(arg, "encode")) {
      g_flags.encode = true;
      continue;
    } else if (!strcmp(arg, "fail-if-unsandboxed")) {
      g_flags.fail_if_unsandboxed = true;
      continue;
    } else if (!strcmp(arg, "large-window")) {
      g_flags.large_window = true;
      continue;
    } else if (!strncmp(arg, "lgwin=", 6)) {
      char* end = NULL;
      long n = strtol(arg + 6, &end, 10);
      if ((end == arg + 6) || (*end != '\x00') || (n < 10) || (n > 30)) {
        return "main: bad -lgwin flag value";
      }
      g_flags.lgwin = (uint32_t)n;
      continue;
    } else if (!strncmp(arg, "quality=", 8)) {
      char* end = NULL;
      long n = strtol(arg + 8, &end, 10);
      if ((end == arg + 8) || (*end != '\x00') || (n < 0) || (n > 11)) {
        return "main: bad -quality flag value";
      }
      g_flags.quality = (uint32_t)n;
      continue;
    }

    return "main: unrecognized flag argument";
  }

  if ((g_flags.lgwin > 24) && !g_flags.large_window) {
    return "main: -lgwin above 24 needs -large-window";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

// ----

// ignore_return_value suppresses errors from -Wall -Werror.
static void  //
ignore_return_value(int ignored) {}

const char*  //
initialize_io_transformer(void) {
  wuffs_base__status status;
  if (g_flags.encode) {
    status = wuffs_brotli__encoder__initialize(
        &g_codec.enc, sizeof g_codec.enc, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (!wuffs_base__status__is_ok(&status)) {
      return wuffs_base__status__message(&status);
    }
    g_io_transformer =
        wuffs_brotli__encoder__upcast_as__wuffs_base__io_transformer(
            &g_codec.enc);
    wuffs_base__io_transformer__set_quirk(
        g_io_transformer, WUFFS_BROTLI__QUIRK_ENCODER_QUALITY, g_flags.quality);
    wuffs_base__io_transformer__set_quirk(
        g_io_transformer, WUFFS_BROTLI__QUIRK_LGWIN, g_flags.lgwin);
  } else {
    status = wuffs_brotli__decoder__initialize(
        &g_codec.dec, sizeof g_codec.dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (!wuffs_base__status__is_ok(&status)) {
      return wuffs_base__status__message(&status);
    }
    g_io_transformer =
        wuffs_brotli__decoder__upcast_as__wuffs_base__io_transformer(
            &g_codec.dec);
  }
  if (g_flags.large_window) {
    wuffs_base__io_transformer__set_quirk(g_io_transformer,
                                          WUFFS_BROTLI__QUIRK_LARGE_WINDOW, 1);
  }

  // The workbuf_len depends on the quirks. At qualities 10 and 11, the
  // encoder's maximum (the C code's worst case) can be far larger than what
  // real inputs need, so use the minimum (a practical bound) there. An input
  // that needs more than that fails with "#bad workbuf length".
  wuffs_base__range_ii_u64 wbl =
      wuffs_base__io_transformer__workbuf_len(g_io_transformer);
  uint64_t n = (g_flags.encode && (g_flags.quality >= 10)) ? wbl.min_incl
                                                           : wbl.max_incl;
  if (n > SIZE_MAX) {
    return "main: workbuf is too large";
  } else if (n > 0) {
    g_workbuf.ptr = malloc((size_t)n);
    if (!g_workbuf.ptr) {
      return "main: out of memory";
    }
    g_workbuf.len = (size_t)n;
  }
  return NULL;
}

const char*  //
main1(void) {
  wuffs_base__io_buffer dst;
  dst.data.ptr = g_dst_buffer_array;
  dst.data.len = DST_BUFFER_ARRAY_SIZE;
  dst.meta.wi = 0;
  dst.meta.ri = 0;
  dst.meta.pos = 0;
  dst.meta.closed = false;

  wuffs_base__io_buffer src;
  src.data.ptr = g_src_buffer_array;
  src.data.len = SRC_BUFFER_ARRAY_SIZE;
  src.meta.wi = 0;
  src.meta.ri = 0;
  src.meta.pos = 0;
  src.meta.closed = false;

  while (true) {
    const int stdin_fd = 0;
    ssize_t n =
        read(stdin_fd, src.data.ptr + src.meta.wi, src.data.len - src.meta.wi);
    if (n < 0) {
      if (errno != EINTR) {
        return strerror(errno);
      }
      continue;
    }
    src.meta.wi += n;
    if (n == 0) {
      src.meta.closed = true;
    }

    while (true) {
      wuffs_base__status status = wuffs_base__io_transformer__transform_io(
          g_io_transformer, &dst, &src, g_workbuf);

      if (dst.meta.ri < dst.meta.wi) {
        // TODO: handle EINTR and other write errors; see "man 2 write".
        const int stdout_fd = 1;
        ignore_return_value(write(stdout_fd, g_dst_buffer_array + dst.meta.ri,
                                  dst.meta.wi - dst.meta.ri));
        dst.meta.ri = dst.meta.wi;
        // Neither the brotli decoder nor encoder re-reads dst (their
        // dst_history_retain_length is 0), as the decoder keeps its history
        // in the workbuf.
        wuffs_base__io_buffer__compact(&dst);
      }

      if (status.repr == wuffs_base__suspension__short_read) {
        break;
      }
      if (status.repr == wuffs_base__suspension__short_write) {
        continue;
      }
      return wuffs_base__status__message(&status);
    }

    wuffs_base__io_buffer__compact(&src);
  }
}

int  //
compute_exit_code(const char* status_msg) {
  if (!status_msg) {
    return 0;
  }
  size_t n = strnlen(status_msg, 2047);
  if (n >= 2047) {
    status_msg = "main: internal error: error message is too long";
    n = strlen(status_msg);
  }
  const int stderr_fd = 2;
  ignore_return_value(write(stderr_fd, status_msg, n));
  ignore_return_value(write(stderr_fd, "\n", 1));
  // Return an exit code of 1 for regular (foreseen) errors, e.g. badly
  // formatted or unsupported input.
  //
  // Return an exit code of 2 for internal (exceptional) errors, e.g. defensive
  // run-time checks found that an internal invariant did not hold.
  return strstr(status_msg, "internal error:") ? 2 : 1;
}

int  //
main(int argc, char** argv) {
  const char* z = parse_flags(argc, argv);
  if (!z) {
    if (g_flags.remaining_argc > 0) {
      z = "main: bad argument: use \"program < input\", not \"program input\"";
    } else {
      z = initialize_io_transformer();
    }
  }

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  // Enter the sandbox after the (malloc) workbuf allocation.
  if (!z) {
    g_sandboxed = prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) >= 0;
    if (!g_sandboxed) {
      const int stderr_fd = 2;
      const char* msg = "SECCOMP_MODE_STRICT sandbox failed.\n";
      ignore_return_value(write(stderr_fd, msg, strlen(msg)));
    }
  }
#endif

  if (!z) {
    if (g_flags.fail_if_unsandboxed && !g_sandboxed) {
      z = "main: unsandboxed";
    } else {
      z = main1();
    }
  }
  int exit_code = compute_exit_code(z);

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  // Call SYS_exit explicitly, instead of calling SYS_exit_group implicitly by
  // either calling _exit or returning from main. SECCOMP_MODE_STRICT allows
  // only SYS_exit.
  syscall(SYS_exit, exit_code);
#endif
  return exit_code;
}
