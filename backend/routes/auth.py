from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from backend.models import User
from backend.models import Session as user_session_registration
from backend.utils import get_db_session, password_verification, token
from backend.schemas import UserCredentials

auth_router = APIRouter(prefix="/auth", tags=["auth"])


@auth_router.post("/login")
async def login(user_credentials: UserCredentials, session: Session = Depends(get_db_session)):
    """Rota para login de usuário."""

    # fazendo buscas no DB pela existência do nome de usuário
    user = session.query(User).filter(User.username == user_credentials.username).first()

    if user:
        # Verificação da senha
        if password_verification(user_credentials.password, user.password_hash):
            # Adicionar seção no DB
            new_session = user_session_registration(user.id)
            session.add(new_session)
            session.commit()

            # Criar token para seção...
            access_token = token(user.id)

            return {
                "detail": {
                    "message": "User successfully logged in.",
                    "access_token": access_token,
                    "token_type": "Beaver",
                    "login": True,
                }
            }
        else:
            raise HTTPException(status_code=400, detail={"message": "Incorrect password.", "login": False})
    else:
        raise HTTPException(status_code=400, detail={"message": "User not found.", "login": False})
