#include "message.h"

/* Undo IRCv3 tag escaping in place: \: \s \\ \r \n */
static void _unescape_tag(char *value) {
    char *out;

    out = value;
    for (; *value != '\0'; ++value) {
        if (*value != '\\') {
            *out++ = *value;
            continue;
        }
        switch (*++value) {
        case ':':  *out++ = ';';  break;
        case 's':  *out++ = ' ';  break;
        case 'r':  *out++ = '\r'; break;
        case 'n':  *out++ = '\n'; break;
        case '\0': --value;       break;
        default:   *out++ = *value;
        }
    }
    *out = '\0';
}

/* Keep the two tags chatlog cares about. */
static void _parse_tags(char *tags, irc_message *message) {
    char *tag, *value;

    for (tag = strtok(tags, ";"); tag != NULL; tag = strtok(NULL, ";")) {
        value = strchr(tag, '=');
        if (value == NULL)
            continue;
        *value++ = '\0';
        _unescape_tag(value);
        if (strcmp(tag, "time") == 0)
            message->time = value;
        else if (strcmp(tag, "account") == 0)
            message->account = value;
    }
}

static char *_skip_spaces(char *head) {
    while (*head == ' ')
        ++head;
    return head;
}

int message_parse(char *line, irc_message *message) {
    char *head, *tags;
    size_t length;

    memset(message, 0, sizeof *message);
    head = line;

    if (*head == '@') {
        tags = head + 1;
        head = strchr(head, ' ');
        if (head == NULL)
            return -1;
        *head++ = '\0';
        _parse_tags(tags, message);
    }

    head = _skip_spaces(head);
    if (*head == ':') {
        message->prefix = ++head;
        head = strchr(head, ' ');
        if (head == NULL)
            return -1;
        *head++ = '\0';
        length = strcspn(message->prefix, "!@");
        if (length >= MESSAGE_NICK)
            length = MESSAGE_NICK - 1;
        memcpy(message->nick, message->prefix, length);
        message->nick[length] = '\0';
    }

    head = _skip_spaces(head);
    message->command = head;
    head = strchr(head, ' ');
    if (head != NULL)
        *head++ = '\0';

    while (head != NULL && message->count < MESSAGE_PARAMETERS) {
        head = _skip_spaces(head);
        if (*head == '\0')
            break;
        if (*head == ':') {
            message->parameters[message->count++] = head + 1;
            break;
        }
        message->parameters[message->count++] = head;
        head = strchr(head, ' ');
        if (head != NULL)
            *head++ = '\0';
    }

    return *message->command == '\0' ? -1 : 0;
}
