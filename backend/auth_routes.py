from fastapi import APIRouter
from sqlalchemy.orm import sessionmaker
from backend.models import db, User

auth_router = APIRouter(prefix="/auth", tags=["auth"])


@auth_router.get("/")
async def home():
    """Rota padrão para autenticação de usuários."""

    return {"Você acessou a rota autenticação."}


@auth_router.post("/create_account")
async def create_account(username: str, password: str):
    # Criando uma seção no DB
    Session = sessionmaker(bind=db)
    session = Session()

    # fazendo buscas no DB pela existência do nome de usuário
    username_exists_in_db = session.query(User).filter(User.username == username).first()

    if not username_exists_in_db:
        new_user = User(username, password)
        session.add(new_user)
        session.commit()
        return {"create_acount": True}
    else:
        return {"create_acount": False}
