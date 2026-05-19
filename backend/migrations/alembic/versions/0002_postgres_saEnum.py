"""postgres_saEnum

Revision ID: 0002_postgres_saEnum
Revises: 346376138365
Create Date: 2026-05-16

Migração que substitui o campo `type` da tabela `events` de
ChoiceType/VARCHAR para o tipo Enum nativo compatível com PostgreSQL.
"""
from typing import Sequence, Union
from alembic import op
import sqlalchemy as sa
from backend.models import EventType

revision: str = "0002_postgres_saEnum"
down_revision: Union[str, None] = "346376138365"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    # Cria o tipo ENUM no PostgreSQL (no SQLite é ignorado)
    event_type_enum = sa.Enum(EventType, name="eventtype")
    event_type_enum.create(op.get_bind(), checkfirst=True)

    with op.batch_alter_table("events") as batch_op:
        batch_op.alter_column(
            "type",
            existing_type=sa.String(length=32),
            type_=event_type_enum,
            existing_nullable=True,
            postgresql_using="type::eventtype",
        )


def downgrade() -> None:
    with op.batch_alter_table("events") as batch_op:
        batch_op.alter_column(
            "type",
            existing_type=sa.Enum(EventType, name="eventtype"),
            type_=sa.String(length=32),
            existing_nullable=True,
        )

    # Remove o tipo ENUM do PostgreSQL (no SQLite é ignorado)
    sa.Enum(name="eventtype").drop(op.get_bind(), checkfirst=True)
