from backend.models import db
from sqlalchemy.orm import sessionmaker
import bcrypt

# from dotenv import load_dotenv
# import os

# Carregando variáveis de ambiente, inclusive do arquivo .env
# load_dotenv()

# Carregando a chave secreta do arquivo .env
# SECRET_KEY = os.getenv("SECRET_KEY")


# Função que inicia uma nova seção no DB e sempre fecha a mesma
def get_db_session():
    try:
        Session = sessionmaker(bind=db)
        session = Session()
        yield session
    finally:
        session.close()


# Gerador de senhas criptografadas
def generate_hash(password: str) -> str:
    password_bytes = password.encode("utf-8")
    hash_bytes = bcrypt.hashpw(password_bytes, bcrypt.gensalt(rounds=12))
    return hash_bytes.decode("utf-8")
