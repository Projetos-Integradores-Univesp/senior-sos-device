from backend.models import db
from sqlalchemy.orm import sessionmaker


# Função que retorna uma nova seção no DB
def get_session():
    try:
        Session = sessionmaker(bind=db)
        session = Session()
        yield session
    finally:
        session.close()
