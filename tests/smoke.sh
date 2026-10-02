#!/bin/bash
# Starts the Welcome offscreen for a few seconds and fails on QML load errors
# (FINAL property clashes and type errors only show up when QML is loaded).
# Usage: smoke.sh <mainuan-welcome binary>
set -u
binary=$1
runtime=$(mktemp -d)
log=$(mktemp)
trap 'rm -rf "$runtime" "$log"' EXIT

env -u QT_LOGGING_RULES XDG_RUNTIME_DIR="$runtime" XDG_CONFIG_HOME="$runtime/config" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software timeout 4 "$binary" >"$log" 2>&1
status=$?

if grep -q 'module "org.kde.kirigami" is not installed' "$log"; then
    echo "Kirigami QML module not installed; skipping"
    exit 77
fi
if [[ $status -ne 124 ]] || grep -E 'qrc:/|Não foi possível carregar|unavailable|Cannot override' "$log"; then
    echo "startup failed (exit $status):"
    cat "$log"
    exit 1
fi
echo "started without QML errors"
