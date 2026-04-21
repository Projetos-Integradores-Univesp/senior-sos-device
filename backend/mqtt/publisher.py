import ssl
import paho.mqtt.publish as publish
from backend.settings import MQTT_CONFIG


def pub_fall(id: int):
    publish.single(
        topic=f"devices/{id}/fall",
        payload="FALL",
        hostname=MQTT_CONFIG["BROKER_URL"],
        qos=MQTT_CONFIG["QOS"],
        port=MQTT_CONFIG["PORT"],
        auth={"username": MQTT_CONFIG["USERNAME"], "password": MQTT_CONFIG["PASSWORD"]},
        tls={"tls_version": ssl.PROTOCOL_TLS_CLIENT},
    )


def pub_button_pressed(id: int):
    publish.single(
        topic=f"devices/{id}/button-pressed",
        payload="BUTTON_PRESSED",
        hostname=MQTT_CONFIG["BROKER_URL"],
        qos=MQTT_CONFIG["QOS"],
        port=MQTT_CONFIG["PORT"],
        auth={"username": MQTT_CONFIG["USERNAME"], "password": MQTT_CONFIG["PASSWORD"]},
        tls={"tls_version": ssl.PROTOCOL_TLS_CLIENT},
    )


if __name__ == "__main__":
    pub_fall(6)
    pub_button_pressed(6)
