from sqlalchemy.orm import sessionmaker
from backend.models import db
from backend.settings import ALGORITHM, ACCESS_TOKEN_EXPIRATION_MINUTES, SECRET_KEY
from datetime import datetime, timezone, timedelta
import bcrypt
import jwt


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


# Função que retorna um token JWT de acesso
def token(user_id: int) -> str:
    expiration_time = (datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_EXPIRATION_MINUTES)).timestamp()
    payload = {"sub": user_id, "expiration_time": expiration_time}
    return jwt.encode(payload, SECRET_KEY, ALGORITHM)
