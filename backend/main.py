from fastapi import FastAPI
from backend.users_routes import users_router

# Iniciando aplicação
app = FastAPI()

# Inclusão de rotas
app.include_router(users_router)
