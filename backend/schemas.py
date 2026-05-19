from pydantic import BaseModel
from datetime import datetime
from enum import Enum


class UserCredentials(BaseModel):
    username: str
    password: str

    class Config:
        from_attributes = True


# Enum espelhando o EventType de models.py para serialização Pydantic correta
class EventType(str, Enum):
    BUTTON_PRESSED = "BUTTON_PRESSED"
    FALL = "FALL"


class DeviceEvent(BaseModel):
    id: int
    # Pydantic converte automaticamente o EventType do SQLAlchemy para str via este Enum
    type: EventType
    time: datetime

    class Config:
        from_attributes = True
        use_enum_values = True
