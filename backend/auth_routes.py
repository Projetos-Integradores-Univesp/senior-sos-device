from fastapi import APIRouter

auth_router = APIRouter(prefix="/auth", tags=["auth"])


@auth_router.get("/")
async def authentication():
    """Rota padrão para autenticação de usuários."""

    return {"Você acessou a rota autenticação."}
