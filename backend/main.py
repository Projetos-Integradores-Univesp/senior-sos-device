from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from backend.routes.users import users_router
from backend.routes.auth import auth_router
from backend.routes.devices import devices_router

# Iniciando aplicação
app = FastAPI()

# Configuração de CORS
origins = [
    "http://localhost:3000",
    "http://127.0.0.1:3000",
    "http://localhost:8080",
    "http://127.0.0.1:8080",
    "http://localhost",
    "http://127.0.0.1",
    "*",  # Permite requisições de qualquer origem (desenvolvimento apenas)
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Inclusão de rotas
app.include_router(users_router)
app.include_router(auth_router)
app.include_router(devices_router)
