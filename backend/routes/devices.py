from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

devices_router = APIRouter(prefix="/devices", tags=["devices"])


@devices_router.get("/")
async def get_devices():
    pass


@devices_router.post("/")
async def add_device():
    pass


@devices_router.put("/{id}")
async def edit_device(id: int):
    pass


@devices_router.delete("/{id}")
async def delete_device(id: int):
    pass


@devices_router.get("/{id}/users")
async def get_users(id: int):
    pass


@devices_router.post("/{id}/users")
async def add_user(id: int):
    pass


@devices_router.delete("/{id}/users/{user_id}")
async def delete_user(id: int, user_id: int):
    pass


@devices_router.get("/{id}/events")
async def get_events(id: int):
    pass
