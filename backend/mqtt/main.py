from backend.settings import MQTT_BROKER_CONFIG
from backend.mqtt.mqtt_client_connection import MqttClientConnection


def start():
    new_connection = MqttClientConnection(
        MQTT_BROKER_CONFIG["HOST"],
        MQTT_BROKER_CONFIG["PORT"],
        MQTT_BROKER_CONFIG["CLIENT_NAME"],
        MQTT_BROKER_CONFIG["KEEPALIVE"],
    )
    new_connection.start_connection()


if __name__ == "__main__":
    start()
