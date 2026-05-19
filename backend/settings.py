from dotenv import load_dotenv
import os

# Em produção, defina DEBUG=false no ambiente do sistema (EnvironmentFile do systemd).
# Em desenvolvimento, mantenha DEBUG=true para carregar o arquivo .env local.
DEBUG = "false"

if DEBUG:
    load_dotenv(dotenv_path=os.path.join(os.path.dirname(__file__), ".env"))

# Configurações JWT
SECRET_KEY = os.getenv("SECRET_KEY")
ALGORITHM = os.getenv("SECRET_ALGORITHM")
ACCESS_TOKEN_EXPIRATION_MINUTES = float(os.getenv("SECRET_ACCESS_TOKEN_EXPIRATION_MINUTES"))

# URL do banco de dados (PostgreSQL em produção, SQLite em dev se preferir)
# Exemplo PostgreSQL: postgresql://elderguard:senha@localhost:5432/elderguard_db
# Exemplo SQLite:     sqlite:///backend/database.db
DATABASE_URL = os.getenv("SECRET_DATABASE_URL")

# Mantidos por compatibilidade com o alembic/env.py (que lê ALEMBIC_DB_LINK)
MODELS_DB_LINK = DATABASE_URL
ALEMBIC_DB_LINK = DATABASE_URL

# Configurações do broker MQTT
MQTT_CONFIG = {
    "BROKER_URL": os.getenv("SECRET_MQTT_BROKER"),
    "PORT": int(os.getenv("SECRET_MQTT_PORT")),  # (TCP) 1883, (SSL/TLS) 8883
    "CLIENT_ID": "backend-senior-sos-device-subscriber",
    "KEEPALIVE": 60,
    "USERNAME": os.getenv("SECRET_MQTT_USERNAME"),
    "PASSWORD": os.getenv("SECRET_MQTT_PASSWORD"),
    "TOPICS": {
        "BUTTON_PRESSED": "devices/+/button-pressed",
        "FALL": "devices/+/fall",
    },
    "QOS": 2,
}
