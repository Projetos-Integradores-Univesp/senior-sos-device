import ssl
import paho.mqtt.client as mqtt
from sqlalchemy.orm import Session
from backend.settings import MQTT_CONFIG
from backend.utils import get_db_session
from backend.models import Device, Event, EventType


class Subscriber:
    def __init__(self):
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=MQTT_CONFIG["CLIENT_ID"],
            protocol=mqtt.MQTTv5,
        )

        self.client.username_pw_set(MQTT_CONFIG["USERNAME"], MQTT_CONFIG["PASSWORD"])

        if MQTT_CONFIG["PORT"] == 8883:
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
            # loop_forever() inicia o loop de rede e mantém o subscriber vivo.
            # O subscribe() agora acontece dentro do on_connect, garantindo
            # que a sessão já está estabelecida quando a inscrição é feita.
            self.client.loop_forever()

        except Exception as e:
            print(f"Falha na conexão. Erro: {e}")

    def on_connect(self, client, userdata, connect_flags, reason_code, properties=None):
        if reason_code.is_failure:
            print(f"Falha ao conectar: {reason_code}")
            return

        print(f"Conectado ao broker. Código: {reason_code}")

        # Inscrição feita AQUI — dentro do on_connect — garante que
        # também funciona após reconexões automáticas do loop_forever.
        client.subscribe(MQTT_CONFIG["TOPICS"]["BUTTON_PRESSED"], qos=MQTT_CONFIG["QOS"])
        client.subscribe(MQTT_CONFIG["TOPICS"]["FALL"], qos=MQTT_CONFIG["QOS"])

    def on_connect_fail(self, client, userdata):
        print("Falha na conexão com o broker.")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode("utf-8")
        device_id = topic.split("/")[1]
        print(f"Mensagem recebida — Device: {device_id}, Payload: {payload}")

        try:
            event_type = EventType[payload]
        except KeyError:
            print(f"Tipo de evento desconhecido: '{payload}'. Ignorando.")
            return

        session: Session = next(get_db_session())
        device: Device = session.query(Device).filter(Device.id == int(device_id)).first()

        if device:
            try:
                event = Event(device.id, event_type)
                session.add(event)
                session.commit()
                print(f"Evento '{payload}' gravado para device id={device_id}.")
            except Exception as e:
                session.rollback()
                print(f"Erro ao gravar evento: {e}")
            finally:
                session.close()
        else:
            print(f"Dispositivo id={device_id} não encontrado no banco.")

    def on_subscribe(self, client, userdata, mid, reason_code_list, properties=None):
        if any(rc.is_failure for rc in reason_code_list):
            print(f"Falha ao inscrever: {reason_code_list}")
        else:
            print(f"Inscrito com sucesso. mid={mid}")


if __name__ == "__main__":
    Subscriber()
