#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -x "$ROOT/build-linux/out/bin/X_Y" ]]; then
    EXE="$ROOT/build-linux/out/bin/X_Y"
elif [[ -x "$ROOT/build/out/bin/X_Y" ]]; then
    EXE="$ROOT/build/out/bin/X_Y"
else
    echo "找不到 X_Y，先构建" >&2
    exit 1
fi

OUT="$ROOT/build/test-fixtures"
mkdir -p "$OUT"

CC="${CC:-gcc}"
INCLUDE="$ROOT/Engine/include"
FIXTURES="$ROOT/test/fixtures"

"$CC" -std=c11 -shared -fPIC -o "$OUT/echo_service.so" \
    "$FIXTURES/echo_service.c" "-I$INCLUDE" "-I$FIXTURES"
"$CC" -std=c11 -shared -fPIC -o "$OUT/echo_client.so" \
    "$FIXTURES/echo_client.c" "-I$INCLUDE" "-I$FIXTURES"

SERVICE="$OUT/echo_service.so"
CLIENT="$OUT/echo_client.so"

SCRIPT="engine create a
engine load a $SERVICE
engine load a $CLIENT
engine services a
engine run a echo hello
engine share a echo
engine shared
engine unload a echo_service
engine unshare echo
engine shared
engine unload a echo_service
engine destroy a
exit"

set +e
OUTPUT="$(printf '%s\n' "$SCRIPT" | "$EXE" 2>&1)"
CODE=$?
set -e

if [[ $CODE -ne 0 ]]; then
    echo "X_Y 退出码 $CODE" >&2
    echo "$OUTPUT" >&2
    exit 1
fi

check() {
    if ! grep -Fq "$1" <<<"$OUTPUT"; then
        echo "输出缺少 '$1'" >&2
        echo "--- output ---" >&2
        echo "$OUTPUT" >&2
        exit 1
    fi
}

check "created engine a"
check "hello"
check "echo"
check "shared service echo from engine a"
check "PROVIDER"
check "还有共享服务"
check "unloaded echo_service"
check "容器没有共享服务"
check "destroyed engine a"

echo "container smoke: OK"
