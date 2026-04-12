from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy import update
from backend.models import User
from backend.utils import get_db_session, token_validation
from backend.utils import generate_hash
from backend.schemas import UserCredentials

users_router = APIRouter(prefix="/users", tags=["users"])


@users_router.post("/")
async def create_account(user_credentials: UserCredentials, session: Session = Depends(get_db_session)):
    """Rota para criação de novo usuário."""

    # Fazendo buscas no DB pela existência do nome de usuário
    username_exists_in_db = session.query(User).filter(User.username == user_credentials.username).first()

    # Criando novo usuário se não existir
    if not username_exists_in_db:
        password_hash = generate_hash(user_credentials.password)
        new_user = User(user_credentials.username, password_hash)
        session.add(new_user)
        session.commit()
        return {"detail": {"message": "User successfully registered.", "created_acount": True}}
    else:
        raise HTTPException(status_code=400, detail={"message": "User already registered.", "created_acount": False})


@users_router.put("/me")
async def update_account(
    new_credentials: UserCredentials, session: Session = Depends(get_db_session), user: User = Depends(token_validation)
):
    """
    Rota para atualização do "username" e "password" do usuário.
    Necessário enviar novo "username" e "password".
    Necessário enviar no header dessa requisição:

        headers = {
            "Authorization": "Bearer SEU_ACCESS_TOKEN_AQUI",
            "Content-Type": "application/json"
        }

    Em "SEU_ACCESS_TOKEN_AQUI" substituir pelo "access_token" do usuário.
    """

    # Fazendo buscas no DB pela existência do nome de usuário
    username_exists_in_db = session.query(User).filter(User.username == new_credentials.username).first()

    if not username_exists_in_db:
        # Construindo a instrução de atualização
        statement = (
            update(User)
            .where(User.id == user.id)
            .values(username=new_credentials.username, password_hash=generate_hash(new_credentials.password))
        )

        session.execute(statement)
        session.commit()

        return {"detail": {"message": "User updated successfully.", "updated": True}}
    else:
        raise HTTPException(status_code=400, detail={"message": "Username already registered.", "updated": False})


@users_router.delete("/me")
async def delete_account(session: Session = Depends(get_db_session), user: User = Depends(token_validation)):
    """
    Rota para exclusão da conta do usuário.
    Necessário enviar no header dessa requisição:

        headers = {
            "Authorization": "Bearer SEU_ACCESS_TOKEN_AQUI",
            "Content-Type": "application/json"
        }

    Em "SEU_ACCESS_TOKEN_AQUI" substituir pelo "access_token" do usuário.
    """

    # Fazendo buscas no DB pelo usuário
    user = session.query(User).filter(User.id == user.id).first()

    # Apagando registro no DB
    session.delete(user)
    session.commit()

    return {"detail": {"message": "User successfully deleted.", "deleted": True}}
