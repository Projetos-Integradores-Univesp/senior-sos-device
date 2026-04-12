from pydantic import BaseModel


class UserCredentials(BaseModel):
    username: str
    password: str

    class Config:
        from_attributes = True
