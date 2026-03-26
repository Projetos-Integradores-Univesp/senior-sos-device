from sqlalchemy import create_engine
from sqlalchemy import Column  # ForeignKey
from sqlalchemy import Integer, String, DateTime, Boolean
from sqlalchemy import func
from sqlalchemy.orm import declarative_base

# from sqlalchemy_utils.types import ChoiceType

# Conexão ado banco de dados
# Passar link do banco de dados quando em produção aqui
db = create_engine("sqlite:///backend/database.db")

# Base do banco de dados
base = declarative_base()


# Tabela Users
class User(base):
    # Nome da tabela no banco de dados
    __tablename__ = "users"

    id = Column(name="id", type_=Integer, primary_key=True, autoincrement=True)
    username = Column(name="username", type_=String(128), unique=True, nullable=False)
    password_hash = Column(name="password_hash", type_=String(256), nullable=False)
    created_at = Column(name="created_at", type_=DateTime, default=func.now())
    status = Column(name="status", type_=Boolean, default=True)

    def __init__(self, username, password_hash, status=True):
        self.username = username
        self.password_hash = password_hash
        self.status = status
