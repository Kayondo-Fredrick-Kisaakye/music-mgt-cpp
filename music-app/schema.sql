-- Active: 1793781295101@@127.0.0.1@5432@music_mgt_cpp
-- Run once against your database:
--   psql "$DATABASE_URL" -f schema.sql

CREATE TABLE IF NOT EXISTS songs (
    id            INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title         VARCHAR(200) NOT NULL,
    artist        VARCHAR(200) NOT NULL,
    album         VARCHAR(200) NOT NULL DEFAULT '',
    genre         VARCHAR(100) NOT NULL DEFAULT '',
    release_year  INTEGER      NOT NULL CHECK (release_year BETWEEN 1900 AND 2100),
    rating        INTEGER      NOT NULL CHECK (rating BETWEEN 1 AND 5),
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_songs_artist ON songs (artist);
