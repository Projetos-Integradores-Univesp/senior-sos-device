#!/bin/bash
# setup_server.sh — Run on your VPS at iot.gtpc.com.br
# Configures Mosquitto + nginx for the ElderGuard dashboard
set -euo pipefail

echo "=== ElderGuard Server Setup ==="

# ---- Mosquitto ----
echo "[1/5] Configuring Mosquitto..."
sudo cp mosquitto/elderguard.conf /etc/mosquitto/conf.d/
sudo cp mosquitto/acl /etc/mosquitto/acl

# Create password file with two users
echo "  Creating MQTT users (you'll be prompted for passwords)..."
sudo mosquitto_passwd -c /etc/mosquitto/passwd elderguard
sudo mosquitto_passwd    /etc/mosquitto/passwd dashboard

sudo systemctl restart mosquitto
sudo systemctl enable mosquitto
echo "  Mosquitto running with auth + ACL"

# ---- Dashboard files ----
echo "[2/5] Deploying dashboard..."
sudo mkdir -p /var/www/elderguard
sudo cp dashboard/index.html /var/www/elderguard/
sudo chown -R www-data:www-data /var/www/elderguard

# ---- Nginx ----
echo "[3/5] Configuring nginx..."
sudo cp nginx/elderguard.conf /etc/nginx/sites-available/elderguard
sudo ln -sf /etc/nginx/sites-available/elderguard /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
echo "  nginx configured with WebSocket proxy"

# ---- TLS (optional) ----
echo "[4/5] TLS certificate..."
if ! sudo test -f /etc/letsencrypt/live/iot.gtpc.com.br/fullchain.pem; then
    echo "  No cert found. To get one, run:"
    echo "  sudo certbot --nginx -d iot.gtpc.com.br"
else
    echo "  TLS certificate already exists"
fi

# ---- Firewall ----
echo "[5/5] Firewall rules..."
sudo ufw allow 1883/tcp comment 'MQTT'
sudo ufw allow 443/tcp  comment 'HTTPS'
sudo ufw allow 80/tcp   comment 'HTTP (redirect)'
sudo ufw reload 2>/dev/null || true

echo ""
echo "=== Setup complete ==="
echo ""
echo "Test MQTT:  mosquitto_sub -h localhost -u dashboard -P <pass> -t 'elderguard/#' -v"
echo "Dashboard:  https://iot.gtpc.com.br"
echo ""
echo "Next steps:"
echo "  1. Update passwords in config.h (MQTT_PASSWORD) and index.html (dashboard password)"
echo "  2. Flash the ESP32 firmware"
echo "  3. Open the dashboard and watch it come alive"
