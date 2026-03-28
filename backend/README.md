# Comandos para iniciar o servidor

Para iniciar o servidor backend, executar o comando: uvicorn backend.main:app --reload

# Para fazer migrações, seguir os passos:

## Configurações iniciais

    1 - No terminal rodar: cd backend/migrations/
    2 - No terminal rodar: alembic init alembic
    3 - Editar o link do DB dentro do arquivo alembic.ini
        3.1 - linha 90: sqlalchemy.url = sqlite:///../database.db
    4 - Editar o arquivo env.py
        4.1 - Acrescentar as linhas:
            import os, sys
            BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
            sys.path.append(BASE_DIR)
            from backend.models import Base
        4.2 - Modificar a linha 27: target_metadata = Base.metadata

## Criar, executar e editar uma migração

    1 - No terminal rodar: cd backend/migrations/
    2 - No terminal rodar o comando: alembic revision --autogenerate -m "titulo_da_migração"
    3 - Editar o arquivo python de migração dentro da pasta migrations/alembic/versions
        3.1 - Alterar o campo "type" dentro da tabela "events":
            de: sqlalchemy_utils.types.choice.ChoiceType(length=255)
            para: sa.String(length=32)
    4 - Executar a migração com: alembic upgrade head