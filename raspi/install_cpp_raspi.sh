#!/bin/sh
set -eu

APP_DIR="/opt/light-engine"
DEPLOY_DIR="${1:-/tmp/light-engine-deploy}"
CONF_DIR="/etc/conf.d"

mkdir -p "$APP_DIR/web" "$APP_DIR/shows" "$APP_DIR/fixtures" "$CONF_DIR" /var/log

install -m 755 "$DEPLOY_DIR/light-engine" "$APP_DIR/light-engine"
install -m 644 "$DEPLOY_DIR/web/index.html" "$DEPLOY_DIR/web/app.css" "$DEPLOY_DIR/web/app.js" "$APP_DIR/web/"
install -m 644 "$DEPLOY_DIR/shows/"*.json "$APP_DIR/shows/"
install -m 644 "$DEPLOY_DIR/fixtures/zkymzl_11ch_moving_head.json" "$APP_DIR/fixtures/"

cat > "$CONF_DIR/light-engine-cpp" <<'EOF'
LIGHT_ENGINE_ARGS="--run-simple-engine 0.0.0.0 80 /opt/light-engine/web 0.0.0.0 9996 127.0.0.1 0"
EOF

printf '%s\n' 'raspi-dmx' > /etc/hostname
hostname raspi-dmx
if ! grep -qE '^[^#]*[[:space:]]raspi-dmx([[:space:]]|$)' /etc/hosts; then
  printf '%s\n' '127.0.1.1 raspi-dmx' >> /etc/hosts
fi

if command -v apk >/dev/null 2>&1; then
  apk add --no-cache avahi dbus >/dev/null
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
  after firewall artnet-dmx-current
}
EOF
chmod 755 /etc/init.d/light-engine-cpp

if command -v rc-update >/dev/null 2>&1; then
  if [ -x /etc/init.d/light-engine ]; then
    rc-service light-engine stop >/dev/null 2>&1 || true
    rc-update del light-engine default >/dev/null 2>&1 || true
  fi
  if [ -x /etc/init.d/dbus ]; then
    rc-update add dbus default >/dev/null || true
    rc-service dbus start >/dev/null 2>&1 || true
  fi
  if [ -x /etc/init.d/avahi-daemon ]; then
    rc-update add avahi-daemon default >/dev/null || true
    rc-service avahi-daemon restart >/dev/null 2>&1 || true
  fi
  if [ -x /etc/init.d/artnet-dmx-current ]; then
    rc-update add artnet-dmx-current default >/dev/null || true
    rc-service artnet-dmx-current restart
  fi
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
echo "Web UI: http://raspi-dmx.local/ (or http://raspi-dmx/ when local name resolution is available)"
