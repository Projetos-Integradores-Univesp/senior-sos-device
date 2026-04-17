import paho.mqtt.client as mqtt


class MqttClientConnection:
    def __init__(self, broker_ip: str, port: int, client_name: str, keepalive: int = 60):
        self.__broker_ip = broker_ip
        self.__port = port
        self.__client_name = client_name
        self.__keepalive = keepalive

        self.__mqtt_client = self.start_connection()
        self.__mqtt_client.loop_start()

    def start_connection(self):
        mqtt_client = mqtt.Client()
        mqtt_client.connect(host=self.__broker_ip, port=self.__port, keepalive=self.__keepalive)
        # mqtt_client.loop_start()
        return mqtt_client
