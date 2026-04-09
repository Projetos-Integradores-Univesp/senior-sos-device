from fastapi import Depends, HTTPException
from fastapi.security import OAuth2PasswordBearer as OA2PB
from sqlalchemy.orm import sessionmaker, Session
from backend.models import db, User
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
def token(user_id: int, expiration_minutes: float = ACCESS_TOKEN_EXPIRATION_MINUTES) -> str:
    expiration_time = (datetime.now(timezone.utc) + timedelta(minutes=expiration_minutes)).timestamp()
    payload = {"user_id": user_id, "expiration_time": expiration_time}
    return jwt.encode(payload, SECRET_KEY, ALGORITHM)


# Função que verifica a validade do token e retorna um "payload" decodificado
def token_validation(
    token: str = Depends(OA2PB(tokenUrl="auth/login-swagger")), session: Session = Depends(get_db_session)
) -> User:
    try:
        payload = jwt.decode(token, SECRET_KEY, ALGORITHM)
        user_id = payload["user_id"]
        user = session.query(User).filter(User.id == user_id).first()
        return user
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=401, detail="Token expired.")
    except jwt.InvalidTokenError:
        raise HTTPException(status_code=401, detail="Token not valid.")
