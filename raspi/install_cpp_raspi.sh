#!/bin/sh
set -eu

APP_DIR="/opt/light-engine"
DEPLOY_DIR="${1:-/tmp/light-engine-deploy}"
CONF_DIR="/etc/conf.d"

mkdir -p "$APP_DIR" "$CONF_DIR" /var/log

install -m 755 "$DEPLOY_DIR/light-engine" "$APP_DIR/light-engine"

if [ ! -f "$CONF_DIR/light-engine-cpp" ]; then
  cat > "$CONF_DIR/light-engine-cpp" <<'EOF'
LIGHT_ENGINE_ARGS=""
EOF
fi

cat > /etc/init.d/light-engine-cpp <<'EOF'
#!/sbin/openrc-run

name="C++ Light Engine"
description="C++ ArtNet light engine"

command="/opt/light-engine/light-engine"
command_args="${LIGHT_ENGINE_ARGS:-}"
command_background="yes"
directory="/opt/light-engine"
pidfile="/run/light-engine-cpp.pid"
output_log="/var/log/light-engine-cpp.log"
error_log="/var/log/light-engine-cpp.err"

depend() {
  need net
  after firewall
}
EOF
chmod 755 /etc/init.d/light-engine-cpp

if command -v rc-update >/dev/null 2>&1; then
  rc-update add light-engine-cpp default >/dev/null || true
  rc-service light-engine-cpp restart
fi

cat > /usr/local/bin/light-engine-cpp-status <<'EOF'
#!/bin/sh
echo "Service:"
rc-service light-engine-cpp status || true
echo
echo "Recent log:"
tail -n 40 /var/log/light-engine-cpp.log 2>/dev/null || true
tail -n 40 /var/log/light-engine-cpp.err 2>/dev/null || true
EOF
chmod 755 /usr/local/bin/light-engine-cpp-status

echo "C++ light engine installed."
