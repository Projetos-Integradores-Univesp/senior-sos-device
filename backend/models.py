from sqlalchemy import create_engine
from sqlalchemy import Column, ForeignKey
from sqlalchemy import Integer, String, DateTime, Boolean
from sqlalchemy import func
from sqlalchemy.orm import declarative_base
from sqlalchemy_utils.types import ChoiceType

# Conexão ado banco de dados
# Passar link do banco de dados quando em produção aqui
db = create_engine("sqlite:///backend/database.db")

# Base do banco de dados
Base = declarative_base()


# Tabela Users
class User(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "users"

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    username = Column("username", String(128), unique=True, nullable=False)
    password_hash = Column("password_hash", String(256), nullable=False)
    created_at = Column("created_at", DateTime, default=func.now())
    status = Column("status", Boolean, default=True)

    def __init__(self, username, password_hash, status=True):
        self.username = username
        self.password_hash = password_hash
        self.status = status


# Tabela Sessions
class Session(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "sessions"

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    user = Column("user", ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    login_time = Column("login_time", DateTime, default=func.now())
    logout_time = Column("logout_time", DateTime)

    def __init__(self, user: User):
        self.user = user


# Tabela Devices
class Device(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "devices"

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    admin = Column("admin", ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    nickname = Column("nickname", String(128), unique=True, nullable=False)
    created_at = Column("created_at", DateTime, default=func.now())

    def __init__(self, admin: User, nickname):
        self.admin = admin
        self.nickname = nickname


# Tabela Events
class Event(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "events"

    EVENTS_TYPES = (("BUTTON PRESSED", "BUTTON PRESSED"), ("FALL", "FALL"))

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    device = Column("device", ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    type = Column("type", ChoiceType(EVENTS_TYPES))
    time = Column("created_at", DateTime, default=func.now())

    def __init__(self, device: Device, type="BUTTON PRESSED"):
        self.device = device
        self.type = type


# Tabela Have
class Have(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "have"

    user = Column("user", ForeignKey("users.id"), primary_key=True)
    device = Column("device", ForeignKey("devices.id"), primary_key=True)

    def __init__(self, user: User, device: Device):
        self.user = user
        self.device = device
