from pydantic import BaseModel
from typing import Optional


class UserSchema(BaseModel):
    username: str
    password: str
    status: Optional[bool]

    class Config:
        from_attributes = True
