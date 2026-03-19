# Para iniciar o servidor backend, executar o comando: uvicorn backend.main:app --reload

from fastapi import FastAPI
from backend.auth_routes import auth_router

app = FastAPI()
app.include_router(auth_router)
