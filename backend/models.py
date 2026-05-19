import enum
from sqlalchemy import create_engine, Enum as SAEnum
from sqlalchemy import ForeignKey, String, DateTime
from sqlalchemy.orm import declarative_base, relationship, Mapped, mapped_column
from datetime import datetime, timezone
from backend.settings import MODELS_DB_LINK


# Link do banco de dados
# pool_pre_ping=True garante que conexões inativas sejam revalidadas (essencial em PostgreSQL).
db = create_engine(MODELS_DB_LINK, pool_pre_ping=True)

# Base do banco de dados
Base = declarative_base()


# Enum nativo para tipos de evento (compatível com PostgreSQL e SQLite)
class EventType(enum.Enum):
    BUTTON_PRESSED = "BUTTON_PRESSED"
    FALL = "FALL"


# Tabela Users
class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    username: Mapped[str] = mapped_column(String(128), unique=True, nullable=False)
    password_hash: Mapped[str] = mapped_column(String(256), nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    status: Mapped[bool] = mapped_column(default=True)

    # Relationships
    sessions: Mapped[list["Session"]] = relationship(back_populates="user", cascade="all, delete-orphan")
    devices_admin: Mapped[list["Device"]] = relationship(back_populates="admin", cascade="all, delete-orphan")
    devices: Mapped[list["Device"]] = relationship(secondary="have", back_populates="users")

    def __init__(self, username, password_hash, status=True):
        self.username = username
        self.password_hash = password_hash
        self.created_at = datetime.now(timezone.utc)
        self.status = status


# Tabela Sessions
class Session(Base):
    __tablename__ = "sessions"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    login_time: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    logout_time: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=True)

    # Relationship
    user: Mapped[list["User"]] = relationship(back_populates="sessions")

    def __init__(self, user_id: int):
        self.user_id = user_id
        self.login_time = datetime.now(timezone.utc)


# Tabela Devices
class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    user_id_admin: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    nickname: Mapped[str] = mapped_column(String(128), unique=True, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))

    # Relationships
    admin: Mapped[list["User"]] = relationship(back_populates="devices_admin")
    events: Mapped[list["Event"]] = relationship(back_populates="device", cascade="all, delete-orphan")
    users: Mapped[list["User"]] = relationship(secondary="have", back_populates="devices")

    def __init__(self, user_id_admin: int, nickname):
        self.user_id_admin = user_id_admin
        self.nickname = nickname
        self.created_at = datetime.now(timezone.utc)


# Tabela Events
class Event(Base):
    __tablename__ = "events"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    # SAEnum nativo: compatível com PostgreSQL (cria tipo ENUM no DB) e SQLite (armazena como VARCHAR)
    type: Mapped[EventType] = mapped_column(SAEnum(EventType))
    time: Mapped[datetime] = mapped_column(DateTime(timezone=True))

    # Relationship
    device: Mapped[list["Device"]] = relationship(back_populates="events")

    def __init__(self, device_id: int, type: EventType = EventType.BUTTON_PRESSED):
        self.device_id = device_id
        self.type = type
        self.time = datetime.now(timezone.utc)


# Tabela Have (relacionamento N:N entre Users e Devices)
class Have(Base):
    __tablename__ = "have"

    user_id: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), primary_key=True)
    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"), primary_key=True)

    def __init__(self, user_id: int, device_id: int):
        self.user_id = user_id
        self.device_id = device_id
