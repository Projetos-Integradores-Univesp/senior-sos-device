import ssl
import paho.mqtt.client as mqtt
from sqlalchemy.orm import Session
from backend.settings import MQTT_CONFIG
from backend.utils import get_db_session
from backend.models import Device, Event


class Subscriber:
    def __init__(self):
        self.client = mqtt.Client(client_id=MQTT_CONFIG["CLIENT_ID"], protocol=mqtt.MQTTv5)
        self.client.username_pw_set(MQTT_CONFIG["USERNAME"], MQTT_CONFIG["PASSWORD"])
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)
        self.client.on_connect = self.on_connect
        self.client.on_connect_fail = self.on_connect_fail
        self.client.on_message = self.on_message
        self.client.on_subscribe = self.on_subscribe

        try:
            self.client.connect(
                MQTT_CONFIG["BROKER_URL"],
                MQTT_CONFIG["PORT"],
                MQTT_CONFIG["KEEPALIVE"],
            )

            # Assinando no tópico de "botão pressionado" e "queda"
            self.client.subscribe(MQTT_CONFIG["TOPICS"]["BUTTON_PRESSED"], qos=MQTT_CONFIG["QOS"])
            self.client.subscribe(MQTT_CONFIG["TOPICS"]["FALL"], qos=MQTT_CONFIG["QOS"])

            # Acrescentar log aqui...
            print("Backend MQTT conectado ao broker. Iniciando loop...")

            rc = 0
            while rc == 0:
                rc = self.client.loop()

            # Acrescentar log aqui..."
            print(f"rc: {str(rc)}")

        except Exception as e:
            # Acrescentar log aqui..
            print(f"Falha na conexão. Erro: {e}")
        finally:
            self.client.loop_stop()

    # Callback quando se conectar ao broker
    def on_connect(self, client, userdata, flags, reason_code, properties=None):
        # Acrescentar log aqui...
        print("Status da conexão: " + str(reason_code))

    def on_connect_fail(self, mqttc, obj):
        # Acrescentar log aqui...
        print("Falha na conexão")

    # Callback quando receber mensagem
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode("utf-8")
        device_id = (topic.split("/"))[1]
        print(f"Device ID: {device_id}, Mensagem: {payload}")

        # Acrescentar informações ao banco de dados.
        session: Session = next(get_db_session())
        device: Device = session.query(Device).filter(Device.id == int(device_id)).first()

        if device:
            try:
                event = Event(device.id, payload)
                session.add(event)
                session.commit()
            finally:
                session.close()
                # Acrescentar log aqui...
                print(f"Evento {payload} adicionado ao dispositivo com id = {device_id}.")
        else:
            # Acrescentar log aqui...
            print(f"Dispositivo com id = {device_id} não encontrado.")

    def on_subscribe(self, mqttc, obj, mid, reason_code_list, properties):
        # Acrescentar log aqui...
        print("Inscrito no tópico: " + str(mid) + " " + str(reason_code_list))


if __name__ == "__main__":
    Subscriber()
