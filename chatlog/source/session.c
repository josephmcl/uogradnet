#include "session.h"
#include "irc.h"
#include "channels.h"
#include "database.h"

#define SESSION_CAPS 4096
#define SESSION_NICK 64
#define SASL_SIZE 512

typedef struct {
    char nick[SESSION_NICK];        /* our nick, as the server sees it */
    char caps[SESSION_CAPS];        /* every capability the server offers */
    int  sasl_failed;
    int  welcomed;                  /* got 001 */
    int  registered;                /* sent NickServ REGISTER */
} session_info;

static session_info TheSession = {0};

/* ---------------------------------------------------------- helpers */

static int _is_channel(const char *name) {
    return name != NULL && (name[0] == '#' || name[0] == '&');
}

static int _is_me(irc_message *message) {
    return message->nick[0] != '\0'
        && strcasecmp(message->nick, TheSession.nick) == 0;
}

/* Is word one of the space separated words in list? ("sasl" in
   "server-time sasl=PLAIN,EXTERNAL" counts.) */
static int _has_word(const char *list, const char *word) {
    const char *at;
    size_t length;
    char after;

    length = strlen(word);
    for (at = list; (at = strstr(at, word)) != NULL; at += length) {
        after = at[length];
        if ((at == list || at[-1] == ' ')
            && (after == '\0' || after == ' ' || after == '='))
            return 1;
    }
    return 0;
}

static void _base64(const unsigned char *in, size_t length, char *out) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i;

    for (i = 0; i + 2 < length; i += 3) {
        *out++ = alphabet[in[i] >> 2];
        *out++ = alphabet[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
        *out++ = alphabet[((in[i + 1] & 15) << 2) | (in[i + 2] >> 6)];
        *out++ = alphabet[in[i + 2] & 63];
    }
    if (i < length) {
        *out++ = alphabet[in[i] >> 2];
        if (i + 1 < length) {
            *out++ = alphabet[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
            *out++ = alphabet[(in[i + 1] & 15) << 2];
        } else {
            *out++ = alphabet[(in[i] & 3) << 4];
            *out++ = '=';
        }
        *out++ = '=';
    }
    *out = '\0';
}

static void _join(const char *channel) {
    if (_is_channel(channel) && strpbrk(channel, " ,\x07") == NULL)
        Irc.send("JOIN %s", channel);
}

/* IRC_CHANNELS, then any channel the bot was invited into before. */
static void _join_all(void) {
    char list[2048], *channel;

    snprintf(list, sizeof list, "%s", Settings.info->channels);
    for (channel = strtok(list, ","); channel != NULL; channel = strtok(NULL, ",")) {
        while (*channel == ' ')
            ++channel;
        if (*channel != '\0')
            _join(channel);
    }
    Database.saved_channels(_join);
}

/* First run only: SASL failed because the bot's account doesn't exist
   yet, so create it. Needs both the failure and the welcome (001),
   which can arrive in either order. */
static void _maybe_register(void) {
    settings_info *settings = Settings.info;

    if (!TheSession.sasl_failed || !TheSession.welcomed || TheSession.registered)
        return;
    if (*settings->email == '\0' || strcasecmp(TheSession.nick, settings->account) != 0)
        return;
    notice_c("registering account %s with NickServ", settings->account);
    Irc.send("PRIVMSG NickServ :REGISTER %s %s", settings->password, settings->email);
    TheSession.registered = 1;
}

/* ---------------------------------------------------------- handlers */

static void _ping(irc_message *message) {
    Irc.send("PONG :%s", message->count ? message->parameters[0] : "");
}

static void _cap(irc_message *message) {
    const char *wanted[] = { "server-time", "account-tag", "multi-prefix", "sasl" };
    char request[256] = "";
    const char *sub, *last;
    size_t used, i, count;

    if (message->count < 3)
        return;
    sub = message->parameters[1];
    last = message->parameters[message->count - 1];

    if (strcasecmp(sub, "LS") == 0) {
        used = strlen(TheSession.caps);
        snprintf(TheSession.caps + used, sizeof TheSession.caps - used, " %s", last);
        if (strcmp(message->parameters[2], "*") == 0)
            return; /* more LS lines are coming */

        /* Only ask for sasl when there is a password to log in with. */
        count = *Settings.info->password ? 4 : 3;
        for (i = 0; i < count; ++i) {
            if (!_has_word(TheSession.caps, wanted[i]))
                continue;
            if (*request != '\0')
                strcat(request, " ");
            strcat(request, wanted[i]);
        }
        if (*request != '\0')
            Irc.send("CAP REQ :%s", request);
        else
            Irc.send("CAP END");
    }
    else if (strcasecmp(sub, "ACK") == 0) {
        if (*Settings.info->password && _has_word(last, "sasl"))
            Irc.send("AUTHENTICATE PLAIN");
        else
            Irc.send("CAP END");
    }
    else if (strcasecmp(sub, "NAK") == 0) {
        Irc.send("CAP END");
    }
}

/* SASL PLAIN: base64("account\0account\0password") */
static void _authenticate(irc_message *message) {
    const char *account = Settings.info->account;
    const char *password = Settings.info->password;
    unsigned char plain[SASL_SIZE];
    char encoded[SASL_SIZE * 4 / 3 + 4];
    size_t a, p;

    if (message->count == 0 || strcmp(message->parameters[0], "+") != 0)
        return;
    a = strlen(account);
    p = strlen(password);
    if (2 * a + p + 2 > sizeof plain) {
        Irc.send("AUTHENTICATE *");
        return;
    }
    memcpy(plain, account, a);
    plain[a] = '\0';
    memcpy(plain + a + 1, account, a);
    plain[2 * a + 1] = '\0';
    memcpy(plain + 2 * a + 2, password, p);
    _base64(plain, 2 * a + 2 + p, encoded);
    Irc.send("AUTHENTICATE %s", encoded);
}

static void _sasl_done(irc_message *message) {
    if (strcmp(message->command, "903") == 0) {
        notice_c("logged in to services as %s", Settings.info->account);
    } else {
        warning_c("SASL login failed (%s); continuing without an account",
                  message->command);
        TheSession.sasl_failed = 1;
        _maybe_register();
    }
    Irc.send("CAP END");
}

static void _nick_in_use(irc_message *message) {
    if (message->count < 2)
        return;
    snprintf(TheSession.nick, sizeof TheSession.nick, "%.60s_", message->parameters[1]);
    Irc.send("NICK %s", TheSession.nick);
}

static void _welcome(irc_message *message) {
    if (message->count == 0)
        return;
    snprintf(TheSession.nick, sizeof TheSession.nick, "%s", message->parameters[0]);
    notice_c("registered as %s", TheSession.nick);
    Irc.send("MODE %s +B", TheSession.nick); /* mark as a bot */
    TheSession.welcomed = 1;
    _maybe_register();
    _join_all();
}

/* NAMES: everyone already in a channel we just joined. */
static void _names(irc_message *message) {
    const char *channel;
    char *nick;

    if (message->count < 4 || !Channels.has(message->parameters[2]))
        return;
    channel = message->parameters[2];
    for (nick = strtok(message->parameters[3], " "); nick; nick = strtok(NULL, " ")) {
        nick += strspn(nick, "~&@%+");
        nick[strcspn(nick, "!")] = '\0';
        if (*nick != '\0')
            Channels.add(channel, nick);
    }
}

static void _join_event(irc_message *message) {
    const char *channel;

    if (message->count == 0)
        return;
    channel = message->parameters[0];
    if (_is_me(message)) {
        Channels.joined(channel);
        notice_c("joined %s", channel);
        return;
    }
    if (!Channels.has(channel))
        return;
    Channels.add(channel, message->nick);
    Database.record(message->time, channel, message->nick, message->account,
                    "join", NULL);
}

static void _part(irc_message *message) {
    const char *channel;

    if (message->count == 0)
        return;
    channel = message->parameters[0];
    if (_is_me(message)) {
        Channels.left(channel);
        return;
    }
    if (!Channels.has(channel))
        return;
    Channels.remove(channel, message->nick);
    Database.record(message->time, channel, message->nick, message->account,
                    "part", message->count > 1 ? message->parameters[1] : NULL);
}

static void _kick(irc_message *message) {
    const char *channel, *target;
    char body[1024];

    if (message->count < 2)
        return;
    channel = message->parameters[0];
    target = message->parameters[1];
    if (message->count > 2 && *message->parameters[2] != '\0')
        snprintf(body, sizeof body, "%s: %s", target, message->parameters[2]);
    else
        snprintf(body, sizeof body, "%s", target);
    Database.record(message->time, channel, message->nick, message->account,
                    "kick", body);

    if (strcasecmp(target, TheSession.nick) == 0) {
        Channels.left(channel);
        Database.forget_channel(channel);
        notice_c("kicked from %s by %s", channel, message->nick);
    } else {
        Channels.remove(channel, target);
    }
}

/* QUIT and NICK name no channel: log them in every channel the
   user was in. */
static void _quit(irc_message *message) {
    const char *channel;
    size_t i;

    for (i = 0; i < Channels.count(); ++i) {
        channel = Channels.name(i);
        if (Channels.remove(channel, message->nick))
            Database.record(message->time, channel, message->nick,
                            message->account, "quit",
                            message->count ? message->parameters[0] : NULL);
    }
}

static void _nick(irc_message *message) {
    const char *channel, *renamed;
    size_t i;

    if (message->count == 0)
        return;
    renamed = message->parameters[0];
    if (_is_me(message))
        snprintf(TheSession.nick, sizeof TheSession.nick, "%s", renamed);
    for (i = 0; i < Channels.count(); ++i) {
        channel = Channels.name(i);
        if (!Channels.remove(channel, message->nick))
            continue;
        Channels.add(channel, renamed);
        Database.record(message->time, channel, message->nick,
                        message->account, "nick", renamed);
    }
}

static void _topic(irc_message *message) {
    if (message->count < 2)
        return;
    Database.record(message->time, message->parameters[0], message->nick,
                    message->account, "topic", message->parameters[1]);
}

static void _invite(irc_message *message) {
    const char *channel;

    if (message->count < 2 || !Settings.info->invites)
        return;
    channel = message->parameters[1];
    if (!_is_channel(channel))
        return;
    notice_c("invited to %s by %s", channel, message->nick);
    Database.remember_channel(channel);
    _join(channel);
}

/* PRIVMSG and NOTICE. Private messages, op-only (@#chan) messages and
   server notices are never logged. Of the CTCPs only /me is kept. */
static void _message(irc_message *message) {
    const char *target, *kind;
    char *text, *action;

    if (message->count < 2)
        return;
    target = message->parameters[0];
    text = message->parameters[1];
    if (!_is_channel(target) || message->prefix == NULL
        || strchr(message->prefix, '!') == NULL)
        return;

    if (text[0] == '\x01') {
        if (message->command[0] != 'P' || strncasecmp(text + 1, "ACTION", 6) != 0)
            return;
        action = text + 7;
        while (*action == ' ')
            ++action;
        action[strcspn(action, "\x01")] = '\0';
        Database.record(message->time, target, message->nick, message->account,
                        "action", action);
        return;
    }

    kind = message->command[0] == 'P' ? "message" : "notice";
    Database.record(message->time, target, message->nick, message->account,
                    kind, text);
}

/* ---------------------------------------------------------- dispatch */

static const struct {
    const char *command;
    void ( *handle)(irc_message *message);
} TheHandlers[] = {
    { "PING",         _ping         },
    { "CAP",          _cap          },
    { "AUTHENTICATE", _authenticate },
    { "903",          _sasl_done    },  /* RPL_SASLSUCCESS */
    { "904",          _sasl_done    },  /* ERR_SASLFAIL    */
    { "905",          _sasl_done    },  /* ERR_SASLTOOLONG */
    { "906",          _sasl_done    },  /* ERR_SASLABORTED */
    { "907",          _sasl_done    },  /* ERR_SASLALREADY */
    { "908",          _sasl_done    },  /* RPL_SASLMECHS   */
    { "433",          _nick_in_use  },
    { "001",          _welcome      },
    { "353",          _names        },
    { "JOIN",         _join_event   },
    { "PART",         _part         },
    { "KICK",         _kick         },
    { "QUIT",         _quit         },
    { "NICK",         _nick         },
    { "TOPIC",        _topic        },
    { "INVITE",       _invite       },
    { "PRIVMSG",      _message      },
    { "NOTICE",       _message      }
};

void session_line(char *line) {
    irc_message message;
    size_t i;

    if (message_parse(line, &message) != 0)
        return;
    for (i = 0; i < sizeof TheHandlers / sizeof *TheHandlers; ++i) {
        if (strcmp(message.command, TheHandlers[i].command) == 0) {
            TheHandlers[i].handle(&message);
            return;
        }
    }
}

void session_begin(void) {
    memset(&TheSession, 0, sizeof TheSession);
    snprintf(TheSession.nick, sizeof TheSession.nick, "%s", Settings.info->nick);
    Channels.reset();

    Irc.send("CAP LS 302");
    Irc.send("NICK %s", TheSession.nick);
    Irc.send("USER %s 0 * :chatlog logger bot", Settings.info->nick);
}

const struct session Session = {
    .begin = session_begin,
    .line  = session_line
};
