### Instalação
Para instalar o broker mosquito no Linux:
```bash
sudo apt-get install mosquitto
sudo apt-get install mosquitto-clients
```
---
### Status, start e stop do broker
Para ver o status do broker:
```bash
systemctl status mosquitto
```
Para iniciar o serviço:
```bash
sudo service mosquitto start
```
Para para o serviço:
```bash
sudo service mosquitto stop
```
Se você quiser ver as mensagens de controle no console, você precisará iniciar o broker mosquitto manualmente pela linha de comando.
Primeiro precisará paralisar o broker, depois digitar o comando:
```bash
mosquitto -v
```
---
### Teste de comunicação
Para fazer um teste de comunicação, com um terminal aberto, se inscreva em um tópico de teste:
```bash
mosquitto_sub -h localhost -t teste
```
Com outro terminal aberto, publique uma mensagem no mesmo tópico de teste:
```bash
mosquitto_pub -h localhost -t teste -m "Hello world!"
```
### Como rodar main.py do módulo mqtt
No terminal, na raiz do projeto, digitar:
```bash
python -m backend.mqtt.subscriber
```
