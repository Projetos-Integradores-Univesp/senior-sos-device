from fastapi import APIRouter, Depends, HTTPException
from fastapi.security import OAuth2PasswordRequestForm as OA2PRF
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


@auth_router.post("/login-swagger")
async def login_swagger(user_credentials: OA2PRF = Depends(), session: Session = Depends(get_db_session)):
    """Rota para login de usuário pelo botão Authorize do Swagger."""

    # fazendo buscas no DB pela existência do nome de usuário
    user = session.query(User).filter(User.username == user_credentials.username).first()

    if user:
        # Verificação da senha
        if password_verification(user_credentials.password, user.password_hash):
            # Cria token para a seção...
            access_token = token(user.id)

            # Registrando seção do usuário no DB
            new_session = user_session_registration(user.id)
            session.add(new_session)
            session.commit()

            return {"access_token": access_token, "token_type": "Bearer"}
        else:
            raise HTTPException(status_code=400)
    else:
        raise HTTPException(status_code=400)


@auth_router.post("/logout")
async def logout():
    """Rota para fazer logout de usuário."""

    pass


@auth_router.get("/refresh")
async def refresh(user: User = Depends(token_validation)):
    """
    Rota para fazer refresh da seção e receber um novo "access_token".
    Necessário enviar o header nessa requisição:

        headers = {
            "Authorization": "Bearer SEU_TOKEN_AQUI",
            "Content-Type": "application/json"
        }

    Em "SEU_TOKEN_AQUI" substituir pelo "refresh_token" do usuário.
    """

    # Criando novo token
    access_token = token(user.id)

    return {"access_token": access_token, "token_type": "Bearer"}
