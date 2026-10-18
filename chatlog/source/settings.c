#include "io.h"

static settings_info TheSettings = {0};

static const char *_environment(const char *name, const char *otherwise) {
    const char *value;

    value = getenv(name);
    if (value == NULL || *value == '\0')
        return otherwise;
    return value;
}

void settings_load(void) {
    TheSettings.host     = _environment("IRC_HOST", "127.0.0.1");
    TheSettings.port     = _environment("IRC_PORT", "6667");
    TheSettings.nick     = _environment("IRC_NICK", "chatlog");
    TheSettings.account  = _environment("IRC_ACCOUNT", TheSettings.nick);
    TheSettings.password = _environment("IRC_PASSWORD", "");
    TheSettings.email    = _environment("IRC_REGISTER_EMAIL", "");
    TheSettings.channels = _environment("IRC_CHANNELS", "");
    TheSettings.database = _environment("DATABASE_URL", "dbname=chatlog");
    TheSettings.invites  = _environment("ALLOW_INVITES", "1")[0] == '1';
    TheSettings.debug    =
        strcasecmp(_environment("LOG_LEVEL", "info"), "debug") == 0;
}

const struct settings Settings = {
    .info = &TheSettings,
    .load = settings_load
};
