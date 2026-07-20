#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
  echo "usage: deploy_raspi.sh <light-engine-binary> <ssh-target> <remote-dir>" >&2
  exit 2
fi

BINARY="$1"
REMOTE="$2"
REMOTE_DIR="$3"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if [ ! -x "$BINARY" ]; then
  echo "binary is missing or not executable: $BINARY" >&2
  exit 1
fi

ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new "$REMOTE" "mkdir -p '$REMOTE_DIR'"
scp -o BatchMode=yes "$BINARY" "$REMOTE:$REMOTE_DIR/light-engine"
scp -o BatchMode=yes "$ROOT_DIR/raspi/install_cpp_raspi.sh" "$REMOTE:$REMOTE_DIR/install_cpp_raspi.sh"
ssh -o BatchMode=yes "$REMOTE" "sh '$REMOTE_DIR/install_cpp_raspi.sh' '$REMOTE_DIR'"
ssh -o BatchMode=yes "$REMOTE" "light-engine-cpp-status"
