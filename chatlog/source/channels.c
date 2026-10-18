#include "channels.h"

#define MEMBERS_SIZE 16

static channel_store TheChannels[CHANNELS_SIZE];
static size_t TheCount = 0;

static void _lower(char *destination, const char *source, size_t size) {
    size_t i;

    for (i = 0; source[i] != '\0' && i + 1 < size; ++i)
        destination[i] = (source[i] >= 'A' && source[i] <= 'Z')
            ? (char) (source[i] - 'A' + 'a')
            : source[i];
    destination[i] = '\0';
}

static channel_store *_find(const char *name) {
    size_t i;

    for (i = 0; i < TheCount; ++i)
        if (strcasecmp(TheChannels[i].name, name) == 0)
            return &TheChannels[i];
    return NULL;
}

static int _member_index(channel_store *channel, const char *nick) {
    char key[CHANNEL_NAME];
    size_t i;

    _lower(key, nick, sizeof key);
    for (i = 0; i < channel->count; ++i)
        if (strcmp(channel->members[i], key) == 0)
            return (int) i;
    return -1;
}

static void _clear(channel_store *channel) {
    size_t i;

    for (i = 0; i < channel->count; ++i)
        free(channel->members[i]);
    free(channel->members);
    channel->members = NULL;
    channel->count = channel->capacity = 0;
}

void channels_joined(const char *name) {
    channel_store *channel;

    if (_find(name) != NULL || TheCount == CHANNELS_SIZE)
        return;
    channel = &TheChannels[TheCount++];
    memset(channel, 0, sizeof *channel);
    snprintf(channel->name, sizeof channel->name, "%s", name);
}

void channels_left(const char *name) {
    channel_store *channel;

    channel = _find(name);
    if (channel == NULL)
        return;
    _clear(channel);
    *channel = TheChannels[--TheCount];
}

int channels_has(const char *name) {
    return _find(name) != NULL;
}

void channels_add(const char *name, const char *nick) {
    channel_store *channel;
    char key[CHANNEL_NAME], **grown;
    size_t capacity;

    channel = _find(name);
    if (channel == NULL || _member_index(channel, nick) >= 0)
        return;
    if (channel->count == channel->capacity) {
        capacity = channel->capacity ? channel->capacity * 2 : MEMBERS_SIZE;
        grown = realloc(channel->members, capacity * sizeof *grown);
        if (grown == NULL)
            return;
        channel->members = grown;
        channel->capacity = capacity;
    }
    _lower(key, nick, sizeof key);
    channel->members[channel->count++] = strdup(key);
}

int channels_remove(const char *name, const char *nick) {
    channel_store *channel;
    int index;

    channel = _find(name);
    if (channel == NULL)
        return 0;
    index = _member_index(channel, nick);
    if (index < 0)
        return 0;
    free(channel->members[index]);
    channel->members[index] = channel->members[--channel->count];
    return 1;
}

size_t channels_count(void) {
    return TheCount;
}

const char *channels_name(size_t index) {
    return index < TheCount ? TheChannels[index].name : NULL;
}

void channels_reset(void) {
    size_t i;

    for (i = 0; i < TheCount; ++i)
        _clear(&TheChannels[i]);
    TheCount = 0;
}

const struct channels Channels = {
    .joined = channels_joined,
    .left   = channels_left,
    .has    = channels_has,
    .add    = channels_add,
    .remove = channels_remove,
    .count  = channels_count,
    .name   = channels_name,
    .reset  = channels_reset
};
