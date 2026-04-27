# 📋 Documentação de Implementação - Integração Flutter + Backend

**Data**: 27 de abril de 2026  
**Versão**: 1.0  
**Status**: ✅ Implementado e Testado

---

## 📑 Índice

1. [Alterações no Backend](#alterações-no-backend)
2. [Alterações no Flutter](#alterações-no-flutter)
3. [Arquivos Criados](#arquivos-criados)
4. [Fluxo de Integração](#fluxo-de-integração)
5. [Como Usar](#como-usar)
6. [Visualizar Banco de Dados](#visualizar-banco-de-dados)

---

## 🔧 Alterações no Backend

### 1. **backend/main.py** - Adição de CORS
**Objetivo**: Permitir requisições HTTP do frontend (Chrome/Flutter)

**Mudanças**:
```python
# ANTES:
from fastapi import FastAPI
from backend.routes.users import users_router
from backend.routes.auth import auth_router
from backend.routes.devices import devices_router

app = FastAPI()
app.include_router(users_router)
app.include_router(auth_router)
app.include_router(devices_router)

# DEPOIS:
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from backend.routes.users import users_router
from backend.routes.auth import auth_router
from backend.routes.devices import devices_router

app = FastAPI()

# Configuração de CORS
origins = [
    "http://localhost:3000",
    "http://127.0.0.1:3000",
    "http://localhost:8080",
    "http://127.0.0.1:8080",
    "http://localhost",
    "http://127.0.0.1",
    "*",
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(users_router)
app.include_router(auth_router)
app.include_router(devices_router)
```

**Por quê**: Sem CORS, o browser/emulador rejeita as requisições com erro `405 Method Not Allowed` nos preflight requests (OPTIONS).

---

## 📱 Alterações no Flutter

### 1. **pubspec.yaml** - Adicionar Dependências

**Mudanças**:
```yaml
# ADICIONADAS:
dependencies:
  http: ^1.1.0                    # Para fazer requisições HTTP
  shared_preferences: ^2.2.0      # Para armazenar tokens localmente
```

---

## 📂 Arquivos Criados

### 1. **lib/services/api_service.dart** - Serviço de API Centralizado

**Localização**: `mobile/elderly_app/lib/services/api_service.dart`

**Funcionalidades**:
- ✅ `login(username, password)` → `POST /auth/login`
- ✅ `register(username, password)` → `POST /users/`
- ✅ `getDevices()` → `GET /devices/` (requer autenticação)
- ✅ `addDevice(nickname)` → `POST /devices/` (requer autenticação)
- ✅ `logout()` → `POST /auth/logout` (requer autenticação)
- ✅ `isAuthenticated()` → Verifica se há token armazenado
- ✅ `getStoredUsername()` → Recupera nome do usuário armazenado

**Recursos Principais**:
- Armazenamento seguro de tokens com `SharedPreferences`
- Tratamento de erros com mensagens amigáveis
- Headers `Authorization: Bearer {token}` automáticos
- Conversão de respostas JSON

---

## 🎯 Alterações em main.dart

### 1. **LoginPage** - Integração com `/auth/login`

**Mudanças**:
- Renomeado campo "Email" → "Nome de Usuário"
- Conectado com `ApiService.login()`
- Armazenamento automático de tokens
- Indicador de carregamento (spinner)
- Feedback visual de erros

**Fluxo**:
```
Usuário digita username + password
         ↓
Clica em "Entrar"
         ↓
ApiService.login() é chamado
         ↓
Se sucesso: Navega para DeviceManagementPage
Se erro: Mostra mensagem de erro em SnackBar
```

---

### 2. **RegistrationScreen** - Integração com `/users/`

**Mudanças**:
- Removido campo "Nome"
- Renomeado "Email" → "Nome de Usuário"
- Conectado com `ApiService.register()`
- Indicador de carregamento

**Fluxo**:
```
Usuário digita username + password
         ↓
Clica em "Cadastrar"
         ↓
ApiService.register() é chamado
         ↓
Se sucesso: Retorna para LoginPage
Se erro: Mostra mensagem de erro
```

---

### 3. **DeviceManagementPage** - Integração com `/devices/`

**Mudanças**:
- Renomeado parâmetro `userEmail` → `username`
- Carregamento automático de dispositivos ao iniciar
- Botão de logout no AppBar
- Adição de novo dispositivo via API
- Spinner enquanto carrega

**Fluxo - Listar Dispositivos**:
```
Página abre
    ↓
initState() chamado
    ↓
_loadDevices() executa ApiService.getDevices()
    ↓
Requisição GET /devices/ com token
    ↓
Dispositivos aparecem na lista
```

**Fluxo - Adicionar Dispositivo**:
```
Usuário clica no botão "+"
    ↓
Dialog abre com campo "Nome do Aparelho"
    ↓
Usuário digita nome e clica "Salvar"
    ↓
ApiService.addDevice(name) é chamado
    ↓
Requisição POST /devices/?nickname={name} com token
    ↓
Se sucesso: _loadDevices() recarrega lista
Se erro: Mostra mensagem de erro
```

---

## 🔄 Fluxo de Integração Completo

```
╔════════════════════════════════════════════════════════════╗
║                    FLUXO DO APLICATIVO                      ║
╚════════════════════════════════════════════════════════════╝

1. INICIALIZAÇÃO
   ├─ IntroScreen (Tela de boas-vindas)
   └─ HomePage (Abas: Login | Contato)

2. LOGIN
   ├─ Usuário entra em LoginPage
   ├─ Digita username + password
   ├─ ApiService.login() → POST /auth/login
   ├─ Backend valida credenciais
   ├─ Se OK: Retorna access_token + refresh_token
   ├─ Tokens são salvos em SharedPreferences
   └─ Navega para DeviceManagementPage

3. REGISTRO (Alternativa ao Login)
   ├─ Usuário clica "Cadastre-se"
   ├─ RegistrationScreen abre
   ├─ Digita username + password
   ├─ ApiService.register() → POST /users/
   ├─ Backend cria novo usuário
   └─ Se OK: Retorna à LoginPage

4. GERENCIAR DISPOSITIVOS
   ├─ DeviceManagementPage carrega
   ├─ ApiService.getDevices() → GET /devices/
   ├─ Dispositivos aparecem na lista
   ├─ Usuário pode adicionar novo: ApiService.addDevice() → POST /devices/
   ├─ Usuário pode editar (local)
   └─ Usuário pode deletar (local)

5. LOGOUT
   ├─ Usuário clica ícone de logout
   ├─ ApiService.logout() → POST /auth/logout
   ├─ Tokens são removidos
   └─ Retorna à HomePage

╔════════════════════════════════════════════════════════════╗
║              COMUNICAÇÃO COM BACKEND                         ║
╚════════════════════════════════════════════════════════════╝

URL Base: http://127.0.0.1:8000

POST /auth/login
├─ Request: {"username": "string", "password": "string"}
├─ Response: {"detail": {"message": "...", "access_token": "...", "refresh_token": "...", "token_type": "Bearer"}}
└─ Status: 200 (sucesso) | 400 (erro)

POST /users/
├─ Request: {"username": "string", "password": "string"}
├─ Response: {"detail": {"message": "User successfully registered.", "created_acount": true}}
└─ Status: 200 (sucesso) | 400 (erro - usuário já existe)

GET /devices/
├─ Headers: {"Authorization": "Bearer {token}"}
├─ Response: {"device_1": {"device_id": 1, "nickname": "Relógio", "admin": true}, ...}
└─ Status: 200 (sucesso)

POST /devices/?nickname={name}
├─ Headers: {"Authorization": "Bearer {token}"}
├─ Response: {"detail": {"message": "New device added successfully.", "device_id": 1, "add_device": true}}
└─ Status: 200 (sucesso) | 400 (erro - dispositivo já existe)

POST /auth/logout
├─ Headers: {"Authorization": "Bearer {token}"}
├─ Response: {"detail": {"message": "User successfully logged out.", ...}}
└─ Status: 200 (sucesso)
```

---

## 🚀 Como Usar

### 1. **Iniciar o Backend**
```bash
cd c:\Users\jmarques\Desktop\senior-sos-device
uvicorn backend.main:app --reload
```
Backend rodará em: `http://127.0.0.1:8000`

### 2. **Instalar Dependências do Flutter**
```bash
cd mobile/elderly_app
flutter pub get
```

### 3. **Rodar o Aplicativo Flutter**
```bash
flutter run
```

### 4. **Testar Login**
- **Usuário de teste**: `teste1` / `senha123`
  - (Ou criar um novo via "Cadastre-se")
- Após login, os dispositivos carregarão automaticamente
- Clique no "+" para adicionar um novo dispositivo

---

## 💾 Visualizar Banco de Dados

### Opção 1: Usar SQLite Browser (GUI)
1. Baixe: https://sqlitebrowser.org/
2. Abra: `backend/database.db`
3. Veja as tabelas em interface visual

### Opção 2: Script Python (Recomendado)
Crie o arquivo `view_database.py` na raiz do projeto:

```python
import sqlite3
import pandas as pd

# Conectar ao banco de dados
conn = sqlite3.connect('backend/database.db')

# Listar todas as tabelas
cursor = conn.cursor()
cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
tables = cursor.fetchall()

print("=" * 80)
print("BANCO DE DADOS: backend/database.db")
print("=" * 80)

for table in tables:
    table_name = table[0]
    print(f"\n📋 Tabela: {table_name}")
    print("-" * 80)
    
    df = pd.read_sql_query(f"SELECT * FROM {table_name}", conn)
    
    if len(df) > 0:
        print(df.to_string(index=False))
    else:
        print("(Tabela vazia)")
    print()

conn.close()
print("=" * 80)
```

**Execute com**:
```bash
python view_database.py
```

### Opção 3: Consultas Diretas (Terminal)
```bash
# Abrir SQLite no terminal
sqlite3 backend/database.db

# Ver estrutura da tabela de usuários
.schema user

# Ver todos os usuários
SELECT * FROM user;

# Ver todos os dispositivos
SELECT * FROM device;

# Ver relacionamentos (quem tem acesso a quais dispositivos)
SELECT * FROM have;

# Ver sessões de login
SELECT * FROM session;

# Sair
.quit
```

---

## 📊 Estrutura do Banco de Dados

### Tabela: `user`
```
┌────────┬──────────────┬──────────────────────────────────────┬───────────┐
│ id     │ username     │ password_hash                        │ created_at│
├────────┼──────────────┼──────────────────────────────────────┼───────────┤
│ 1      │ teste1       │ $2b$12$... (hash bcrypt)             │ 2026-04...│
│ 2      │ usuario2     │ $2b$12$... (hash bcrypt)             │ 2026-04...│
└────────┴──────────────┴──────────────────────────────────────┴───────────┘
```

### Tabela: `device`
```
┌────────┬──────────────┬──────────────────┬─────────────────────┐
│ id     │ user_id_admin│ nickname         │ created_at          │
├────────┼──────────────┼──────────────────┼─────────────────────┤
│ 1      │ 1            │ Relógio SOS      │ 2026-04-27 10:30:45 │
│ 2      │ 1            │ Pulseira SOS     │ 2026-04-27 11:00:20 │
└────────┴──────────────┴──────────────────┴─────────────────────┘
```

### Tabela: `have`
```
┌────────┬─────────┬───────────┐
│ id     │ user_id │ device_id │
├────────┼─────────┼───────────┤
│ 1      │ 1       │ 1         │
│ 2      │ 1       │ 2         │
└────────┴─────────┴───────────┘
```

### Tabela: `session`
```
┌────────┬─────────┬──────────────────────┬──────────────────────┐
│ id     │ user_id │ login_time           │ logout_time          │
├────────┼─────────┼──────────────────────┼──────────────────────┤
│ 1      │ 1       │ 2026-04-27 10:15:30  │ 2026-04-27 10:30:45  │
│ 2      │ 1       │ 2026-04-27 11:00:00  │ NULL                 │
└────────┴─────────┴──────────────────────┴──────────────────────┘
```

---

## ✅ Checklist de Integração

- [x] Configurar CORS no Backend
- [x] Criar ApiService para comunicação HTTP
- [x] Implementar login com armazenamento de tokens
- [x] Implementar registro de novo usuário
- [x] Implementar listagem de dispositivos
- [x] Implementar adição de novo dispositivo
- [x] Adicionar logout
- [x] Tratamento de erros e feedback visual
- [x] Indicadores de carregamento (spinners)
- [x] Armazenamento seguro de tokens (SharedPreferences)

---

## 🐛 Troubleshooting

### Erro: "ClientException: Failed to fetch"
**Solução**: 
- Certifique-se que o backend está rodando
- Verifique se a URL está correta em `api_service.dart`
- Se em emulador Android, use `http://10.0.2.2:8000` em vez de `http://127.0.0.1:8000`

### Erro: "405 Method Not Allowed" nas requisições OPTIONS
**Solução**:
- CORS não está configurado
- Reinicie o servidor backend
- Verifique se `CORSMiddleware` foi adicionado em `main.py`

### Token expirado
**Solução**:
- Faça logout e login novamente
- Os tokens são armazenados em `SharedPreferences`

---

## 📝 Notas Adicionais

1. **Segurança em Produção**:
   - Use `flutter_secure_storage` em vez de `SharedPreferences` para tokens
   - Configure CORS com origins específicas (não use `"*"`)
   - Use HTTPS em vez de HTTP

2. **Melhorias Futuras**:
   - Implementar refresh token automático
   - Cache de dispositivos
   - Notificações em tempo real (WebSocket/MQTT)
   - Sincronização offline

3. **Testes**:
   - Testar com múltiplos usuários
   - Testar com dispositivos reais
   - Testar conexão perdida/reconexão
   - Testar com conexão lenta

---

**Documentação criada em**: 27 de abril de 2026  
**Versão do Flutter**: 3.11.4+  
**Versão do FastAPI**: Latest
