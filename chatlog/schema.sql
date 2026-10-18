-- Chat log schema for the chatlog bot.
-- Run as the database owner:  psql -d chatlog -f schema.sql

CREATE TABLE IF NOT EXISTS messages (
    id       bigserial   PRIMARY KEY,
    ts       timestamptz NOT NULL DEFAULT now(),
    channel  text        NOT NULL,
    nick     text        NOT NULL,
    account  text,                       -- NickServ account, if the user was logged in
    kind     text        NOT NULL CHECK (kind IN
               ('message', 'action', 'notice', 'join', 'part', 'quit', 'nick', 'kick', 'topic')),
    body     text,                       -- message text, part/quit reason, new nick, kick target + reason, topic
    search   tsvector GENERATED ALWAYS AS (to_tsvector('english', coalesce(body, ''))) STORED
);

CREATE INDEX IF NOT EXISTS messages_channel_ts ON messages (channel, ts);
CREATE INDEX IF NOT EXISTS messages_nick_ts    ON messages (lower(nick), ts);
CREATE INDEX IF NOT EXISTS messages_search     ON messages USING gin (search);

-- Channels the bot was /INVITEd into, so it rejoins them after a restart.
CREATE TABLE IF NOT EXISTS bot_channels (
    name      text PRIMARY KEY,
    added_at  timestamptz NOT NULL DEFAULT now()
);
