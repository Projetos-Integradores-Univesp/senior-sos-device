from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from backend.models import User
from backend.utils import get_db_session
from backend.utils import generate_hash
from backend.schemas import UserCreate

users_router = APIRouter(prefix="/users", tags=["users"])


@users_router.post("/")
async def create_account(user_create: UserCreate, session: Session = Depends(get_db_session)):
    """Rota para criação de novo usuário."""

    # fazendo buscas no DB pela existência do nome de usuário
    username_exists_in_db = session.query(User).filter(User.username == user_create.username).first()

    # Criando novo usuário se não existir
    if not username_exists_in_db:
        password_hash = generate_hash(user_create.password)
        new_user = User(user_create.username, password_hash)
        session.add(new_user)
        session.commit()
        return {"detail": {"message": "User successfully registered.", "created_acount": True}}
    else:
        raise HTTPException(status_code=400, detail={"message": "User already registered.", "created_acount": False})
