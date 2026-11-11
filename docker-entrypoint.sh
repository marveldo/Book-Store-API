#!/bin/sh
# Runs database migrations, then starts the server.
# Set RUN_MIGRATIONS=false to skip the migrations.
set -e

if [ "${RUN_MIGRATIONS:-true}" = "true" ]; then
    if [ -n "$DB_URL" ]; then
        url="$DB_URL"
    else
        url="postgres://${DB_USER}:${DB_PASSWORD}@${DB_HOST}:${DB_PORT:-5432}/${DB_NAME}"
    fi
    # The pgx5 driver tries SSL first and falls back to plain if the server doesn't offer it.
    url="pgx5://${url#*://}"

    echo "Running migrations..."
    migrate -path ./migrations -database "$url" up
fi

# exec: the server replaces this script, so it receives Render's stop signal directly.
exec ./cpp_server
