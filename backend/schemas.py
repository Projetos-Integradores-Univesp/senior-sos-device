from pydantic import BaseModel

# from typing import Optional


class UserCreate(BaseModel):
    username: str
    password: str

    class Config:
        from_attributes = True


# class UserCreate(BaseModel):
#     username: str
#     password: str
#     status: Optional[bool]
#
#     class Config:
#         from_attributes = True
