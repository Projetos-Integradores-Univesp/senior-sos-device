import json
import paho.mqtt.client as mqtt

BROKER_HOST = "localhost"
BROKER_PORT = 1883

CLIENT_ID = "backend-subscriber"


# Callback quando conecta ao broker
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Conectado ao broker MQTT")

        # Assina telemetria de TODOS os devices
        client.subscribe("devices/+/telemetry", qos=1)

        # Assina status de TODOS os devices
        client.subscribe("devices/+/status", qos=1)
    else:
        print(f"❌ Falha na conexão. Código: {reason_code}")


# Callback quando recebe mensagem
def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8")

    print("\n📩 Mensagem recebida")
    print(f"🧵 Tópico: {topic}")
    print(f"📦 Payload bruto: {payload}")

    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        print("⚠ Payload não é JSON")
        return

    # Extrai o deviceId do tópico
    # Exemplo tópico: devices/device123/telemetry
    parts = topic.split("/")
    device_id = parts[1] if len(parts) >= 2 else "unknown"

    print(f"🆔 Device ID: {device_id}")
    print(f"📊 Dados: {data}")

    # Aqui você pode:
    # - salvar no banco
    # - enviar para outra fila
    # - processar regras
    # - etc.


def main():
    client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv5)

    client.on_connect = on_connect
    client.on_message = on_message

    # Se usar autenticação:
    # client.username_pw_set("usuario", "senha")

    client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)

    print("🚀 Backend MQTT rodando...")
    client.loop_forever()


if __name__ == "__main__":
    main()
