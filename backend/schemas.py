from pydantic import BaseModel

# from typing import Optional


class UserCredentials(BaseModel):
    username: str
    password: str

    class Config:
        from_attributes = True


# class UserCredentials(BaseModel):
#     username: str
#     password: str
#     status: Optional[bool]
#
#     class Config:
#         from_attributes = True
