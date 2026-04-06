from dotenv import load_dotenv
import os

# Em produção mudar para False
DEBUG = True

# Carregando variáveis do arquivo .env
if DEBUG:
    load_dotenv()

# Carregando constantes
SECRET_KEY = os.getenv("SECRET_KEY")
ALGORITHM = os.getenv("ALGORITHM")
ACCESS_TOKEN_EXPIRY_MINUTES = os.getenv("ACCESS_TOKEN_EXPIRY_MINUTES")

# Configuração dos links do DB para models.py e alembic.ini
MODELS_DB_LINK = "sqlite:///backend/database.db"
ALEMBIC_DB_LINK = "sqlite:///../database.db"  # --> substituir manualmente por enquanto...
