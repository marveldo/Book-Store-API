#!/bin/sh
# Runs database migrations, then starts the server.
# Set RUN_MIGRATIONS=false to skip the migrations.
set -e

# Percent-encodes every byte ("p#ss" -> "%70%23%73%73"), so a password containing
# # @ / : ? or spaces can't break the URL. Encoding letters too is allowed and simpler.
urlencode() {
    [ -z "$1" ] && return 0
    printf '%s' "$1" | od -An -v -tx1 | tr -s ' \n' ' ' | sed 's/^ //; s/ $//; s/ /%/g; s/^/%/'
}

if [ "${RUN_MIGRATIONS:-true}" = "true" ]; then
    if [ -n "$DB_URL" ]; then
        url="$DB_URL"   # used as-is: special characters in its password must already be encoded
    else
        url="postgres://$(urlencode "$DB_USER"):$(urlencode "$DB_PASSWORD")@${DB_HOST}:${DB_PORT:-5432}/${DB_NAME}"
    fi
    # The pgx5 driver tries SSL first and falls back to plain if the server doesn't offer it.
    url="pgx5://${url#*://}"

    echo "Running migrations..."
    migrate -path ./migrations -database "$url" up
fi

# exec: the server replaces this script, so it receives Render's stop signal directly.
exec ./cpp_server
