from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import and_
from sqlalchemy.orm import Session
from backend.utils import token_validation, get_db_session
from backend.models import User, Device, Have
from backend.schemas import DeviceEvent
from typing import List


devices_router = APIRouter(prefix="/devices", tags=["devices"])


@devices_router.get("/")
async def get_devices(user: User = Depends(token_validation), session: Session = Depends(get_db_session)):
    """
    Rota para listar todos os dispositivos que o usuário tem acesso.
    Necessário enviar token de autenticação.
    """
    relationships = session.query(Have).filter(Have.user_id == user.id).all()

    devices_info = {}
    for index, relationship in enumerate(relationships):
        this_device = session.query(Device).filter(Device.id == relationship.device_id).first()

        device_info = {
            "device_id": this_device.id,
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

        return {"detail": {"message": "New device added successfully.", "device_id": device.id, "add_device": True}}
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
async def delete_device(id: int, user: User = Depends(token_validation), session: Session = Depends(get_db_session)):
    """
    Rota para excluir dispositivo. Necessário ser o admin do dispositivo.
    Necessário enviar token de autenticação.
    """
    device = session.query(Device).filter(Device.id == id).first()

    if device:
        if device.user_id_admin == user.id:
            session.delete(device)
            session.commit()
            return {"detail": {"message": "Device successfully deleted.", "deleted": True}}
        else:
            raise HTTPException(status_code=403, detail={"message": "You must be device admin."})
    else:
        raise HTTPException(status_code=400, detail="Device not found.")


@devices_router.get("/{id}/users")
async def get_users(id: int, user: User = Depends(token_validation), session: Session = Depends(get_db_session)):
    """
    Rota para listar os usuários com acesso ao dispositivo. Necessário ser o admin do dispositivo.
    Necessário enviar token de autenticação.
    """
    device = session.query(Device).filter(Device.id == id).first()

    if device:
        if device.user_id_admin == user.id:
            relationships = session.query(Have).filter(Have.device_id == id).all()

            users_info = {}
            for index, relationship in enumerate(relationships):
                this_user = session.query(User).filter(User.id == relationship.user_id).first()
                if this_user.id != user.id:
                    users_info[f"user_{index}"] = this_user.username

            return users_info

        else:
            raise HTTPException(status_code=403, detail={"message": "You must be device admin."})
    else:
        raise HTTPException(status_code=400, detail="Device not found.")


@devices_router.post("/{id}/users/{username}")
async def add_access(
    id: int, username: str, user: User = Depends(token_validation), session: Session = Depends(get_db_session)
):
    """
    Rota para adicionar novo acesso de usuário ao dispositivo. Necessário ser o admin do dispositivo.
    Necessário enviar token de autenticação.
    """
    device = session.query(Device).filter(Device.id == id).first()
    new_user = session.query(User).filter(User.username == username).first()

    if device and new_user:
        if device.user_id_admin == user.id:
            # Verificando se existe relacionamento
            if not (new_user in device.users):
                new_relationship = Have(new_user.id, device.id)
                session.add(new_relationship)
                session.commit()
                return {"detail": {"message": "New user access added to device successfully.", "add_access": True}}
            else:
                return {"detail": {"message": "This user already have access to device.", "add_access": False}}
        else:
            raise HTTPException(status_code=403, detail={"message": "You must be device admin."})
    else:
        raise HTTPException(status_code=400, detail="Device or new user not found.")


@devices_router.delete("/{id}/users/{username}")
async def remove_access(
    id: int, username: str, user: User = Depends(token_validation), session: Session = Depends(get_db_session)
):
    """
    Rota para remover o acesso de um usuário a um dispositivo. Necessário enviar token de autenticação.
    Somente o administrador do dispositivo e o usuário com acesso ao dispositivo podem remover o acesso.
    """
    device = session.query(Device).filter(Device.id == id).first()
    user_to_remove = session.query(User).filter(User.username == username).first()
    relationship = (
        session.query(Have).filter(and_(Have.device_id == device.id, Have.user_id == user_to_remove.id)).first()
    )

    if relationship:
        # O administrador do dispositivo solicita a remoção de acesso de outro usuário
        # ... OU ...
        # O usuário não é o administrador, mas solicita para si mesmo a remoção de acesso

        is_admin: bool = device.user_id_admin == user.id
        is_self_removal: bool = user.id == user_to_remove.id

        if is_admin != is_self_removal:
            session.delete(relationship)
            session.commit()
            return {"detail": {"message": "Access removed successfully.", "remove_access": True}}
        else:
            raise HTTPException(
                status_code=403,
                detail={
                    "message": "Admin can't remove yourself. If you are not device admin, you must have access to device."
                },
            )
    else:
        raise HTTPException(status_code=400, detail="Device or user not found.")


@devices_router.get("/{id}/events", response_model=List[DeviceEvent])
async def get_events(id: int, user: User = Depends(token_validation), session: Session = Depends(get_db_session)):
    """Rota que recupera eventos do dispositivo. Necessário enviar token de autenticação."""

    device = session.query(Device).filter(Device.id == id).first()
    user = session.query(User).filter(User.id == user.id).first()

    if device and user:
        relationship = session.query(Have).filter(and_(Have.device_id == device.id, Have.user_id == user.id)).first()
        if relationship:
            return device.events
        else:
            raise HTTPException(status_code=403, detail="User don't have access to device.")
    else:
        raise HTTPException(status_code=400, detail="Device or user not found.")
