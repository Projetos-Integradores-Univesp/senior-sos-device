from pydantic import BaseModel
from datetime import datetime
from enum import Enum


class UserCredentials(BaseModel):
    username: str
    password: str

    class Config:
        from_attributes = True


# Classe para retornar corretamente o atributo type, pois é do tipo "ChoiceType"
class EventType(str, Enum):
    BUTTON_PRESSED = "BUTTON_PRESSED"
    FALL = "FALL"


class DeviceEvent(BaseModel):
    id: int
    type: EventType
    time: datetime

    class Config:
        from_attributes = True
        orm_mode = True
