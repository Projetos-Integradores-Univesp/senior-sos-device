from fastapi import FastAPI
from passlib.context import CryptContext
from dotenv import load_dotenv
import os

# Carregando variáveis de ambiente, inclusive do arquivo .env
load_dotenv()

# Carregando a chave secreta do arquivo .env
SECRET_KEY = os.getenv("SECRET_KEY")

# Iniciando aplicação
app = FastAPI()

# Gerador de senhas criptografadas
bcrypt_context = CryptContext(schemes=["bcrypt"], deprecated="auto")

# Inclusão de rotas
from backend.auth_routes import auth_router

app.include_router(auth_router)
