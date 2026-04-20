import paho.mqtt.client as mqtt
from backend.settings import MQTT_CONFIG


class Subscriber:
    def __init__(self):
        self.client = mqtt.Client(client_id=MQTT_CONFIG["CLIENT_ID"], protocol=mqtt.MQTTv5)
        # self.client.username_pw_set("freemqtt", "public")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

        try:
            self.client.connect(
                MQTT_CONFIG["BROKER_URL"],
                MQTT_CONFIG["PORT"],
                MQTT_CONFIG["KEEPALIVE"],
            )

            # Acrescentar log de Backend MQTT em funcionamento...
            print("Backend MQTT em funcionamento.")

            rc = 0
            while rc == 0:
                rc = self.client.loop()
            print(f"rc: {str(rc)}")

        except Exception as e:
            # Acrescentar log de falha aqui..
            print(f"Falha na conexão. Erro: {e}")
        finally:
            self.client.loop_stop()

    # Callback quando se conectar ao broker
    def on_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            # Assinando no tópico de "botão pressionado"
            self.client.subscribe(MQTT_CONFIG["TOPICS"]["BUTTON_PRESSED"], qos=2)

            # Assinando no tópico de "queda"
            self.client.subscribe(MQTT_CONFIG["TOPICS"]["FALL"], qos=2)

            # Acrescentar log de conexão aqui...
            print("Inscrições no broker MQTT bem sucedidas")
        else:
            # Acrescentar log de falha aqui..
            print(f"Falha na conexão. Código: {reason_code}")

    # Callback quando receber mensagem
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode("utf-8")
        device_id = (topic.split("/"))[1]
        print(f"Device ID: {device_id}, Mensagem: {payload}")

        # Acrescentar informações ao banco de dados...


if __name__ == "__main__":
    Subscriber()
