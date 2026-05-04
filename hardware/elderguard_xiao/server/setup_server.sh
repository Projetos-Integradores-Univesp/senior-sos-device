#!/bin/bash
# setup_server.sh — Executar no VPS (iot.gtpc.com.br)
# Configura Mosquitto + nginx para o ElderGuard XIAO ESP32S3
set -euo pipefail

echo "=== ElderGuard Server Setup (XIAO ESP32S3) ==="

# ---- Mosquitto ----
echo "[1/5] Configurando Mosquitto..."
sudo cp mosquitto/elderguard.conf /etc/mosquitto/conf.d/
sudo cp mosquitto/acl /etc/mosquitto/acl

echo "  Criando usuários MQTT (será solicitada senha para cada um)..."
sudo mosquitto_passwd -c /etc/mosquitto/passwd elderguard   # dispositivo
sudo mosquitto_passwd    /etc/mosquitto/passwd dashboard    # responsável / dashboard

sudo systemctl restart mosquitto
sudo systemctl enable mosquitto
echo "  Mosquitto rodando com autenticação + ACL"

# ---- Dashboard ----
echo "[2/5] Implantando dashboard..."
sudo mkdir -p /var/www/elderguard
sudo cp dashboard/index.html /var/www/elderguard/
sudo chown -R www-data:www-data /var/www/elderguard

# ---- nginx ----
echo "[3/5] Configurando nginx..."
sudo cp nginx/elderguard.conf /etc/nginx/sites-available/elderguard
sudo ln -sf /etc/nginx/sites-available/elderguard /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
echo "  nginx configurado com proxy WebSocket"

# ---- TLS ----
echo "[4/5] Certificado TLS..."
if ! sudo test -f /etc/letsencrypt/live/iot.gtpc.com.br/fullchain.pem; then
    echo "  Certificado não encontrado. Para obter um, execute:"
    echo "  sudo certbot --nginx -d iot.gtpc.com.br"
else
    echo "  Certificado TLS já existe"
fi

# ---- Firewall ----
echo "[5/5] Regras de firewall..."
sudo ufw allow 1883/tcp comment 'MQTT (dispositivo WiFi)'
sudo ufw allow 443/tcp  comment 'HTTPS (dashboard + WSS)'
sudo ufw allow 80/tcp   comment 'HTTP (redirect para HTTPS)'
sudo ufw reload 2>/dev/null || true

echo ""
echo "=== Setup concluído ==="
echo ""
echo "Testar MQTT:"
echo "  mosquitto_sub -h localhost -u dashboard -P <senha> -t 'elderguard/#' -v"
echo ""
echo "Dashboard:"
echo "  https://iot.gtpc.com.br"
echo ""
echo "Testar acknowledge do LED de alerta:"
echo "  mosquitto_pub -h localhost -u dashboard -P <senha> \\"
echo "    -t elderguard/001/ack -m 'ok' -q 1"
echo ""
echo "Próximos passos:"
echo "  1. Editar config.h no firmware: MQTT_PASSWORD, WIFI_SSID, WIFI_PASSWORD"
echo "  2. Gravar firmware no XIAO ESP32S3 via: pio run -e xiao_esp32s3 -t upload"
echo "  3. Abrir o dashboard e aguardar o primeiro heartbeat (≤ 15 min)"
