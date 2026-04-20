from dotenv import load_dotenv
import os

# Em produção mudar para False
DEBUG = True

# Carregando variáveis do arquivo .env
if DEBUG:
    load_dotenv(dotenv_path=os.path.join(os.path.dirname(__file__), ".env"))

# Carregando constantes
SECRET_KEY = os.getenv("SECRET_KEY")
ALGORITHM = os.getenv("ALGORITHM")
ACCESS_TOKEN_EXPIRATION_MINUTES = float(os.getenv("ACCESS_TOKEN_EXPIRATION_MINUTES"))

# Configuração dos links do DB para models.py e alembic.ini
MODELS_DB_LINK = "sqlite:///backend/database.db"
ALEMBIC_DB_LINK = "sqlite:///../database.db"  # --> substituir manualmente por enquanto...

# Configurações do broker MQTT
MQTT_CONFIG = {
    "BROKER_URL": "localhost",  # "broker.freemqtt.com",
    "PORT": 1883,
    "CLIENT_ID": "backend-senior-sos-device-subscriber",
    "KEEPALIVE": 60,
    "USERNAME": "",
    "PASSWORD": "",
    "TOPICS": {"BUTTON_PRESSED": "devices/+/button-pressed", "FALL": "devices/+/fall"},
}
