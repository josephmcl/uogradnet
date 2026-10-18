#include "irc.h"

#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#define SEND_SIZE 1024

static int TheSocket = -1;
static volatile sig_atomic_t TheStop = 0;

int irc_connect(const char *host, const char *port) {
    struct addrinfo hints = {0}, *found, *address;
    int error;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(host, port, &hints, &found);
    if (error != 0) {
        warning_c("could not resolve %s: %s", host, gai_strerror(error));
        return -1;
    }

    TheSocket = -1;
    for (address = found; address != NULL; address = address->ai_next) {
        TheSocket = socket(address->ai_family, address->ai_socktype,
                           address->ai_protocol);
        if (TheSocket < 0)
            continue;
        if (connect(TheSocket, address->ai_addr, address->ai_addrlen) == 0)
            break;
        close(TheSocket);
        TheSocket = -1;
    }
    freeaddrinfo(found);
    return TheSocket < 0 ? -1 : 0;
}

/* printf into one line, strip any CR/LF smuggled in by an argument,
   and write it with the CRLF the protocol wants. */
void irc_send(const char *format, ...) {
    char buffer[SEND_SIZE];
    va_list arguments;
    ssize_t written;
    int length, offset, i;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof buffer - 2, format, arguments);
    va_end(arguments);
    if (length < 0)
        return;
    if (length > (int) sizeof buffer - 3)
        length = (int) sizeof buffer - 3;

    for (i = 0; i < length; ++i)
        if (buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] = ' ';
    debug_c(">> %.*s", length, buffer);
    buffer[length++] = '\r';
    buffer[length++] = '\n';

    for (offset = 0; offset < length; offset += (int) written) {
        written = write(TheSocket, buffer + offset, (size_t) (length - offset));
        if (written < 0) {
            if (errno == EINTR) {
                written = 0;
                continue;
            }
            return; /* run() notices the dead socket */
        }
    }
}

void irc_run(void ( *on_line)(char *line)) {
    static char buffer[IRC_LINE];
    struct pollfd watch;
    size_t have;
    ssize_t got;
    char *start, *end;
    int ready, pinged;

    have = 0;
    pinged = 0;
    while (!TheStop) {
        watch.fd = TheSocket;
        watch.events = POLLIN;
        ready = poll(&watch, 1, IRC_PING_SECONDS * 1000);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (ready == 0) {
            if (pinged) {
                warning_c("%s", "ping timeout");
                return;
            }
            irc_send("PING :chatlog");
            pinged = 1;
            continue;
        }

        got = read(TheSocket, buffer + have, sizeof buffer - 1 - have);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0) {
            warning_c("%s", "server closed the connection");
            return;
        }
        pinged = 0;
        have += (size_t) got;
        buffer[have] = '\0';

        /* Hand over every complete line, keep the partial one. */
        start = buffer;
        while ((end = memchr(start, '\n', have - (size_t) (start - buffer)))) {
            *end = '\0';
            if (end > start && end[-1] == '\r')
                end[-1] = '\0';
            if (*start != '\0') {
                debug_c("<< %s", start);
                on_line(start);
            }
            start = end + 1;
        }
        have -= (size_t) (start - buffer);
        memmove(buffer, start, have);
        if (have == sizeof buffer - 1)
            have = 0; /* an absurdly long line: drop it */
    }
}

void irc_stop(void) {
    TheStop = 1;
}

int irc_stopping(void) {
    return TheStop;
}

void irc_close(void) {
    if (TheSocket >= 0)
        close(TheSocket);
    TheSocket = -1;
}

const struct irc Irc = {
    .connect  = irc_connect,
    .send     = irc_send,
    .run      = irc_run,
    .stop     = irc_stop,
    .stopping = irc_stopping,
    .close    = irc_close
};
