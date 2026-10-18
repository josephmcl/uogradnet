#pragma once

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Log lines go to stderr, which systemd sends to the journal:
       journalctl -u chatlog                                       */
#define notice_c(f_, ...)  fprintf(stderr, ("(notice) "  f_ "\n"), __VA_ARGS__)
#define warning_c(f_, ...) fprintf(stderr, ("(warning) " f_ "\n"), __VA_ARGS__)
#define error_c(f_, ...)   fprintf(stderr, ("(error) "   f_ "\n"), __VA_ARGS__)

/* Raw IRC traffic, only when LOG_LEVEL=debug. */
#define debug_c(f_, ...) do {                                        \
    if (Settings.info->debug)                                        \
        fprintf(stderr, ("(debug) " f_ "\n"), __VA_ARGS__);          \
} while (0)

#include "settings.h"
