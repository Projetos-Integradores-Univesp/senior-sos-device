# Changelog — Backend Senior SOS Device

## Mudanças aplicadas (migração para produção + PostgreSQL)

### `settings.py`
- `DEBUG` agora lê da variável de ambiente `DEBUG` (padrão `true`).
  Em produção, defina `DEBUG=false` no systemd — isso impede o carregamento do `.env` local.
- `MODELS_DB_LINK` e `ALEMBIC_DB_LINK` agora leem `DATABASE_URL` do ambiente,
  suportando PostgreSQL e SQLite transparentemente.
- Porta MQTT agora lê `BROKER_PORT` do ambiente (padrão `8883`).

### `models.py`
- Removida dependência `sqlalchemy_utils` e o `ChoiceType`.
- `EventType` é agora um `enum.Enum` nativo Python.
- O campo `type` da tabela `events` usa `SAEnum(EventType)` do SQLAlchemy,
  que cria um tipo `ENUM` real no PostgreSQL e usa `VARCHAR` no SQLite.
- `db = create_engine(..., pool_pre_ping=True)` — reconecta automaticamente
  após quedas de conexão (essencial em PostgreSQL de longa duração).

### `schemas.py`
- Adicionado `use_enum_values = True` ao `Config` do `DeviceEvent`,
  para serialização correta do `SAEnum` via Pydantic.

### `main.py`
- CORS corrigido: `allow_credentials=True` é inválido com `allow_origins=["*"]`.
  Agora `allow_credentials` é `False` quando `origins == ["*"]`.
- Origens CORS lidas da variável de ambiente `CORS_ORIGINS` (CSV separado por vírgula).
  Ex: `CORS_ORIGINS=https://${SECRET_MQTT_BROKER}`

### `mqtt/subscriber.py`
- Payload MQTT mapeado para `EventType` enum antes de gravar no banco,
  evitando erro de tipo com `SAEnum`.
- Adicionado `session.rollback()` no bloco de exceção ao gravar evento.

### `migrations/alembic/env.py`
- `DATABASE_URL` lida do ambiente — não precisa mais editar `alembic.ini` manualmente.
- Compatível com `.env` local (dev) e variáveis de sistema (produção).

### `migrations/alembic/versions/0002_postgres_saEnum.py` *(novo)*
- Migração que converte o campo `type` de `VARCHAR(32)` para `ENUM` nativo no PostgreSQL.
- Inclui `downgrade()` para reversão.

### `requirements.txt` *(novo)*
- Lista completa de dependências com versões mínimas.
- Inclui `psycopg2-binary` para conexão PostgreSQL.
- Remove `sqlalchemy-utils` (não é mais necessário).

### Arquivos de implantação *(novos)*
- `elderguard-api.service` — unit systemd para a API FastAPI.
- `elderguard-mqtt.service` — unit systemd para o subscriber MQTT.
- `nginx-elderguard.conf` — configuração Nginx com proxy reverso e HTTPS.
- `.env.example` — template de variáveis de ambiente (nunca commite o `.env` real).
