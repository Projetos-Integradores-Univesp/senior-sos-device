from backend.models import db
from sqlalchemy.orm import sessionmaker
import bcrypt


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


# Função que verifica a igualdade da senhas fornecidas
def password_verification(password: str, password_hash: str) -> bool:
    if bcrypt.checkpw(password.encode("utf-8"), password_hash.encode("utf-8")):
        return True
    else:
        return False


# Função que retorna um token de acesso
def token(user_id: int) -> str:
    pass
