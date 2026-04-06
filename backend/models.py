from sqlalchemy import create_engine
from sqlalchemy import Column, ForeignKey
from sqlalchemy import Integer, String, DateTime, Boolean
from sqlalchemy.orm import declarative_base
from sqlalchemy_utils.types import ChoiceType
from datetime import datetime
from backend.settings import MODELS_DB_LINK


# Link do banco de dados
db = create_engine(MODELS_DB_LINK)

# Base do banco de dados
Base = declarative_base()


# Tabela Users
class User(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "users"

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    username = Column("username", String(128), unique=True, nullable=False)
    password_hash = Column("password_hash", String(256), nullable=False)
    created_at = Column("created_at", DateTime)
    status = Column("status", Boolean, default=True)

    def __init__(self, username, password_hash, status=True):
        self.username = username
        self.password_hash = password_hash
        self.created_at = datetime.now()
        self.status = status


# Tabela Sessions
class Session(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "sessions"

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    user_id = Column("user_id", ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    login_time = Column("login_time", DateTime)
    logout_time = Column("logout_time", DateTime)

    def __init__(self, user_id: User):
        self.user_id = user_id
        self.login_time = datetime.now()


# Tabela Devices
class Device(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "devices"

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    user_id_admin = Column("user_admin", ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    nickname = Column("nickname", String(128), unique=True, nullable=False)
    created_at = Column("created_at", DateTime)

    def __init__(self, user_id_admin: User, nickname):
        self.user_id_admin = user_id_admin
        self.nickname = nickname
        self.created_at = datetime.now()


# Tabela Events
class Event(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "events"

    EVENTS_TYPES = (("BUTTON PRESSED", "BUTTON PRESSED"), ("FALL", "FALL"))

    id = Column("id", Integer, primary_key=True, autoincrement=True)
    device_id = Column("device_id", ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    type = Column("type", ChoiceType(EVENTS_TYPES))
    time = Column("created_at", DateTime)

    def __init__(self, device_id: Device, type="BUTTON PRESSED"):
        self.device_id = device_id
        self.type = type
        self.time = datetime.now()


# Tabela Have
class Have(Base):
    # Nome da tabela no banco de dados
    __tablename__ = "have"

    user_id = Column("user_id", ForeignKey("users.id"), primary_key=True)
    device_id = Column("device_id", ForeignKey("devices.id"), primary_key=True)

    def __init__(self, user_id: User, device_id: Device):
        self.user_id = user_id
        self.device_id = device_id
