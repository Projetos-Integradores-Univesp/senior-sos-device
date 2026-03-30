from fastapi import FastAPI
from backend.auth_routes import auth_router

# Iniciando aplicação
app = FastAPI()

# Inclusão de rotas
app.include_router(auth_router)
