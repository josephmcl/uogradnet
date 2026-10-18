#pragma once

#include "io.h"

#define CHANNELS_SIZE 128
#define CHANNEL_NAME 64

/* Who is in which channel, so a QUIT or NICK (which name no channel)
   can be logged in every channel the user was actually in.

   Names are compared case-insensitively (ASCII), like the server does. */
typedef struct {
    char   name[CHANNEL_NAME];
    char **members;             /* lower-cased nicks */
    size_t count;
    size_t capacity;
} channel_store;

struct channels {
    void ( *joined)(const char *channel);   /* the bot joined */
    void ( *left)(const char *channel);     /* the bot left   */
    int  ( *has)(const char *channel);
    void ( *add)(const char *channel, const char *nick);
    /* returns 1 if nick was there */
    int  ( *remove)(const char *channel, const char *nick);
    size_t ( *count)(void);
    const char *( *name)(size_t index);
    void ( *reset)(void);
};

extern const struct channels Channels;
