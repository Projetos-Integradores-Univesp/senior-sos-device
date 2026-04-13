from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from backend.utils import token_validation, get_db_session
from backend.models import User, Device, Event, Have

devices_router = APIRouter(prefix="/devices", tags=["devices"])


@devices_router.get("/")
async def get_devices(user: User = Depends(token_validation), session: Session = Depends(get_db_session)):
    """
    Rota para listar todos os dispositivos que o usuário tem acesso.
    Necessário enviar token de autenticação.
    """
    relationships = session.query(Have).filter(Have.user_id == user.id).all()

    devices_info = {}
    for index, device in enumerate(relationships):
        this_device = session.query(Device).filter(Device.id == device.device_id).first()

        device_info = {
            "nickname": this_device.nickname,
            "admin": (True if this_device.user_id_admin == user.id else False),
        }

        devices_info[f"device_{index + 1}"] = device_info

    return devices_info


@devices_router.post("/")
async def add_device(nickname: str, user: User = Depends(token_validation), session: Session = Depends(get_db_session)):
    """
    Rota para adicionar novos dispositivos. Necessário enviar token de autenticação.
    O usuário que adicionar o novo dispositivo, será o administrador desse dispositivo.
    """

    # Fazendo buscas no DB pela existência do "nickname"
    nickname_exists = session.query(Device).filter(Device.nickname == nickname).first()

    # Adicionando novo dispositivo
    if not nickname_exists:
        new_device = Device(user.id, nickname)
        session.add(new_device)
        session.commit()

        # Preenchendo a tabela "Have"
        device = session.query(Device).filter(Device.nickname == nickname).first()
        new_relationship = Have(user.id, device.id)
        session.add(new_relationship)
        session.commit()

        return {"detail": {"message": "New device added successfully.", "add_device": True}}
    else:
        raise HTTPException(status_code=400, detail={"message": "Device already registered.", "add_device": False})


@devices_router.put("/{id}")
async def edit_device(
    id: int, nickname: str, user: User = Depends(token_validation), session: Session = Depends(get_db_session)
):
    """
    Rota para editar o dispositivo. Necessário ser o admin do dispositivo.
    Necessário enviar token de autenticação.
    """
    device = session.query(Device).filter(Device.id == id).first()
    nickname_exits = session.query(Device).filter(Device.nickname == nickname).first()

    if device.user_id_admin == user.id:
        if not nickname_exits:
            device.nickname = nickname
            session.commit()
            return {"detail": {"message": "Device updated successfully.", "updated": True}}
        else:
            raise HTTPException(status_code=400, detail={"message": "Nickname already registered.", "updated": False})
    else:
        raise HTTPException(status_code=403, detail={"message": "You must be device admin."})


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
