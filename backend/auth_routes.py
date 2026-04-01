from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from backend.models import User
from backend.utils import get_db_session
from backend.utils import generate_hash
from backend.schemas import UserSchema

auth_router = APIRouter(prefix="/auth", tags=["auth"])


@auth_router.get("/")
async def home():
    """Rota padrão para autenticação de usuários."""

    return {"Você acessou a rota autenticação."}


@auth_router.post("/create_account")
async def create_account(user_schema: UserSchema, session: Session = Depends(get_db_session)):
    """Rota para criação de novo usuário."""

    # fazendo buscas no DB pela existência do nome de usuário
    username_exists_in_db = session.query(User).filter(User.username == user_schema.username).first()

    # Criando novo usuário se não existir
    if not username_exists_in_db:
        password_hash = generate_hash(user_schema.password)
        new_user = User(user_schema.username, password_hash)
        session.add(new_user)
        session.commit()
        return {"detail": {"message": "User successfully registered.", "create_acount": True}}
    else:
        raise HTTPException(status_code=400, detail={"message": "User already registered.", "create_acount": False})
