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
    "BROKER_URL": os.getenv("BROKER_URL"),  # "localhost"
    "PORT": 8883,  # (TCP) 1883, (SSL/TLS) 8883
    "CLIENT_ID": "backend-senior-sos-device-subscriber",
    "KEEPALIVE": 60,
    "USERNAME": os.getenv("BROKER_USERNAME"),
    "PASSWORD": os.getenv("BROKER_PASSWORD"),
    "TOPICS": {"BUTTON_PRESSED": "devices/+/button-pressed", "FALL": "devices/+/fall"},
    "QOS": 2,
}
