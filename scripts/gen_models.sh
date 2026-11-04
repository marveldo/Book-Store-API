#!/usr/bin/env bash
# Builds models/model.json from models/model.template.json, then runs drogon_ctl.
#   Values come from: real environment variable  >  .env file.
#   Nothing is exported: this script only READS the environment, so there is nothing to clean up.
set -euo pipefail
cd "$(dirname "$0")/.."                       # project root (this script lives in scripts/)

ENV_FILE=".env"
TEMPLATE="models/model.template.json"
OUTPUT="models/model.json"
[[ -f "$TEMPLATE" ]] || { echo "error: $TEMPLATE not found" >&2; exit 1; }

# 1. read .env into a local table (not exported)
declare -A file_vars
if [[ -f "$ENV_FILE" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%$'\r'}"
        [[ "$line" =~ ^[[:space:]]*(#|$) ]] && continue
        line="${line#export }"
        [[ "$line" == *=* ]] || continue
        key="$(echo -n "${line%%=*}" | xargs)"
        value="$(echo -n "${line#*=}" | sed -E 's/^[[:space:]]+|[[:space:]]+$//g')"
        if [[ "$value" =~ ^\"(.*)\"$ || "$value" =~ ^\'(.*)\'$ ]]; then
            value="${BASH_REMATCH[1]}"
        else
            value="${value%%[[:space:]]#*}"              # strip trailing  # comment
            value="${value%"${value##*[![:space:]]}"}"   # and the spaces before it
        fi
        file_vars["$key"]="$value"
    done < "$ENV_FILE"
else
    echo "note: no $ENV_FILE, using environment variables only" >&2
fi

replace_all() {                               # literal find/replace (no special characters)
    local s="$1" find="$2" repl="$3" out=""
    while [[ "$s" == *"$find"* ]]; do
        out+="${s%%"$find"*}$repl"
        s="${s#*"$find"}"
    done
    printf '%s' "$out$s"
}

json_escape() {                               # make a value safe inside a JSON string
    local s="$1"
    s="$(replace_all "$s" '\' '\\')"
    s="$(replace_all "$s" '"' '\"')"
    printf '%s' "$s"
}

# 2. fill every ${NAME} placeholder found in the template
numeric=(DB_PORT)                             # must become JSON numbers, not strings ("5432" -> 5432)
json="$(cat "$TEMPLATE")"
missing=()
for name in $(grep -o '\${[A-Za-z_][A-Za-z0-9_]*}' "$TEMPLATE" | sort -u | tr -d '${}'); do
    if [[ -n "${!name+x}" ]]; then value="${!name}"            # real environment variable wins
    elif [[ -n "${file_vars[$name]+x}" ]]; then value="${file_vars[$name]}"
    else missing+=("$name"); continue; fi
    if [[ " ${numeric[*]} " == *" $name "* ]]; then
        [[ "$value" =~ ^[0-9]+$ ]] || { echo "error: $name must be a whole number, got '$value'" >&2; exit 1; }
        json="$(replace_all "$json" "\"\${$name}\"" "$value")"   # drop the quotes around it
        json="$(replace_all "$json" "\${$name}" "$value")"
        continue
    fi
    json="$(replace_all "$json" "\${$name}" "$(json_escape "$value")")"
done

if (( ${#missing[@]} )); then
    echo "error: no value for: ${missing[*]} (set them in $ENV_FILE or the environment)" >&2
    exit 1
fi

printf '%s\n' "$json" > "$OUTPUT"
echo "wrote $OUTPUT"

# 3. generate the model classes
drogon_ctl create model models
