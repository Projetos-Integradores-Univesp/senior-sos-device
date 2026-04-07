from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from backend.models import User
from backend.models import Session as user_session_registration
from backend.utils import get_db_session, password_verification, token, token_validation
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
            # Criar tokens para a seção...
            access_token = token(user.id)
            refresh_token = token(user.id, 60 * 24)  # 1 dia

            # Registrando seção do usuário no DB
            new_session = user_session_registration(user.id)
            session.add(new_session)
            session.commit()

            return {
                "detail": {
                    "message": "User successfully logged in.",
                    "access_token": access_token,
                    "refresh_token": refresh_token,
                    "token_type": "Bearer",
                    "login": True,
                }
            }
        else:
            raise HTTPException(status_code=400, detail={"message": "Incorrect password.", "login": False})
    else:
        raise HTTPException(status_code=400, detail={"message": "User not found.", "login": False})


@auth_router.post("/logout")
async def logout():
    """Rota para fazer logout de usuário."""

    pass


@auth_router.get("/refresh")
async def refresh(payload: dict = Depends(token_validation)):
    """Rota para fazer refresh da seção e enviar um novo "access_token"."""

    # Criando novo token
    access_token = token(int(payload["sub"]))

    return {"detail": {"access_token": access_token, "token_type": "Bearer"}}
