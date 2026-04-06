from fastapi import FastAPI
from backend.routes.users import users_router
from backend.routes.auth import auth_router

# Iniciando aplicação
app = FastAPI()

# Inclusão de rotas
app.include_router(users_router)
app.include_router(auth_router)
