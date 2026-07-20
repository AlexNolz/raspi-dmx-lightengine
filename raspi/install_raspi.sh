#!/bin/sh
set -eu

APP_DIR="/opt/light-engine"
DEPLOY_DIR="/tmp/light-engine-deploy"
CONF_DIR="/etc/conf.d"

mkdir -p "$APP_DIR/web" "$CONF_DIR" /var/log

if ! command -v python3 >/dev/null 2>&1 && command -v apk >/dev/null 2>&1; then
  apk add --no-cache python3 >/dev/null
fi

cp "$DEPLOY_DIR/light_engine.py" "$APP_DIR/light_engine.py"
cp "$DEPLOY_DIR/light_engine_config.json" "$APP_DIR/light_engine_config.json"
cp "$DEPLOY_DIR/README_light_engine.md" "$APP_DIR/README_light_engine.md"
cp "$DEPLOY_DIR/web/index.html" "$APP_DIR/web/index.html"
cp "$DEPLOY_DIR/web/app.css" "$APP_DIR/web/app.css"
cp "$DEPLOY_DIR/web/app.js" "$APP_DIR/web/app.js"
chmod 755 "$APP_DIR/light_engine.py"

if [ -x /root/raspi-dmx-node ]; then
  cp "$DEPLOY_DIR/artnet-dmx-current.initd" /etc/init.d/artnet-dmx-current
  chmod 755 /etc/init.d/artnet-dmx-current
  if [ ! -f "$CONF_DIR/artnet-dmx-current" ]; then
    cat > "$CONF_DIR/artnet-dmx-current" <<'EOF'
ARTNET_DMX_CMD="/root/raspi-dmx-node --dmx-device /dev/serial0 --universe 0 --fps 40 --quiet-polls"
EOF
  fi
  rc-update add artnet-dmx-current default >/dev/null || true
fi

if [ ! -f "$CONF_DIR/light-engine" ]; then
  cat > "$CONF_DIR/light-engine" <<'EOF'
WEB_HOST="0.0.0.0"
WEB_PORT="80"
OS2L_HOST="0.0.0.0"
OS2L_PORT="9996"
ARTNET_HOST="127.0.0.1"
ARTNET_UNIVERSE="0"
LED_START_CHANNEL="3"
SEGMENTS="16"
FPS="30"
EOF
fi

cat > /etc/init.d/light-engine <<'EOF'
#!/sbin/openrc-run

name="Mini Light Engine"
description="VirtualDJ OS2L to ArtNet light engine"

command="/usr/bin/python3"
command_args="/opt/light-engine/light_engine.py --web-host ${WEB_HOST:-0.0.0.0} --web-port ${WEB_PORT:-80} --os2l-host ${OS2L_HOST:-0.0.0.0} --os2l-port ${OS2L_PORT:-9996} --artnet-host ${ARTNET_HOST:-127.0.0.1} --universe ${ARTNET_UNIVERSE:-0} --start-channel ${LED_START_CHANNEL:-3} --segments ${SEGMENTS:-8} --fps ${FPS:-30}"
command_background="yes"
directory="/opt/light-engine"
pidfile="/run/light-engine.pid"
output_log="/var/log/light-engine.log"
error_log="/var/log/light-engine.err"

depend() {
  need net
  after firewall
}
EOF
chmod 755 /etc/init.d/light-engine

if command -v rc-update >/dev/null 2>&1; then
  rc-update add light-engine default >/dev/null || true
  if [ -x /etc/init.d/artnet-dmx-current ]; then
    rc-service artnet-dmx-current restart || true
  fi
  rc-service light-engine restart
fi

cat > /usr/local/bin/light-engine-status <<'EOF'
#!/bin/sh
echo "Service:"
rc-service light-engine status || true
echo
echo "Listening ports:"
netstat -ltn 2>/dev/null | grep -E '(:8088|:9996)' || true
echo
echo "Recent log:"
tail -n 40 /var/log/light-engine.log 2>/dev/null || true
tail -n 40 /var/log/light-engine.err 2>/dev/null || true
EOF
chmod 755 /usr/local/bin/light-engine-status

echo "Light engine installed."
if [ -x /etc/init.d/artnet-dmx-current ]; then
  echo "ArtNet-DMX bridge installed."
fi
echo "Web UI: http://raspi-dmx/web"
echo "VirtualDJ os2lDirectIp: raspi-dmx:9996"
