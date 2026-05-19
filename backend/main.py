import os
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from backend.routes.users import users_router
from backend.routes.auth import auth_router
from backend.routes.devices import devices_router

# Iniciando aplicação
app = FastAPI(
    title="Senior SOS Device — API",
    description="API REST para gerenciamento de dispositivos ElderGuard.",
    version="1.0.0",
)

# Configuração de CORS
# Em produção, substitua "*" pelo(s) domínio(s) real(is) da aplicação.
# Ex: ["https://seu-dominio.exemplo.com"]  — definir em SECRET_MQTT_BROKER
_raw_origins = os.getenv("CORS_ORIGINS", "*")
origins = [o.strip() for o in _raw_origins.split(",")] if _raw_origins != "*" else ["*"]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=origins != ["*"],  # credentials=True é inválido com allow_origins=["*"]
    allow_methods=["*"],
    allow_headers=["*"],
)

# Inclusão de rotas
app.include_router(users_router)
app.include_router(auth_router)
app.include_router(devices_router)
