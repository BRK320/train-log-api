# Trainlog-api (C + Mongoose + SQLite)

API REST feita em **C** com **Mongoose** (HTTP server) e **SQLite** (persistência).
Inclui autenticação com **Bearer token**, sessões, e permissões por **roles** (`admin` / `client`).

## Stack
- C (GCC / MinGW)
- Mongoose (HTTP)
- SQLite3
- Windows CNG (`bcrypt`) para RNG e hashing (PBKDF2)

---

## Funcionalidades
- Autenticação:
  - `POST /signup` (cria conta + auto-login)
  - `POST /login`
  - `POST /logout`
  - `GET /me`
- Exercícios:
  - `GET /exercises` (público)
  - `POST /exercises` (admin)
  - `PUT /exercises/:id` (admin)
  - `DELETE /exercises/:id` (admin)
  - `GET /exercises/:id` (público)
- Workouts (por utilizador):
  - `GET /workouts` (user)
  - `GET /workouts/:id` (user)
  - `POST /workouts` (user)
  - `PUT /workouts/:id` (user)
  - `DELETE /workouts/:id` (user)
  - Sets:
    - `POST /workouts/:id/sets` (user)
    - `PUT /workouts/:id/sets/:setId` (user)
    - `DELETE /workouts/:id/sets/:setId` (user)
- Stats:
  - `GET /stats/volume` (user)
  - `GET /stats/prs` (user)
- Admin:
  - `GET /admin/users` (admin)
  - `POST /admin/users` (admin)

---

## Autenticação e permissões
### Bearer Token
- Após login/signup recebes um `token`
- Envia em todas as rotas protegidas:


### Roles
- `client`: acesso aos endpoints de workouts + stats do próprio
- `admin`: pode gerir utilizadores e fazer CRUD de exercícios

---

## Como compilar (Windows + MSYS2 / MinGW)
Exemplo (PowerShell):

```powershell
gcc (Get-ChildItem src\*.c) -o api.exe -lws2_32 -lsqlite3 -lbcrypt
```

---

## Como correr

```
.\api.exe
```

#### A API fica em:
- http://localhost:8000

#### A base de dados SQLite é criada em:
- db/gym.db

---

# Exemplos (curl)

## Health
```bash
curl http://localhost:8000/health
```

## Signup (auto-login)
```bash
curl -X POST http://localhost:8000/signup ^
  -H "Content-Type: application/json" ^
  -d "{ \"email\":\"user@mail.com\", \"password\":\"123\", \"name\":\"Bruno\", \"surname\":\"Silva\" }"
```

## Login
```bash
curl -X POST http://localhost:8000/login ^
  -H "Content-Type: application/json" ^
  -d "{ \"email\":\"user@mail.com\", \"password\":\"123\" }"
```

## Me
```bash
curl http://localhost:8000/me ^
  -H "Authorization: Bearer <TOKEN>"
```
## GET exercises (público)
```bash
curl http://localhost:8000/exercises
```

## Criar exercício (admin)
```bash
curl -X POST http://localhost:8000/exercises ^
  -H "Authorization: Bearer <ADMIN_TOKEN>" ^
  -H "Content-Type: application/json" ^
  -d "{ \"name\":\"Bench Press\" }"
```

## Criar workout (user)
```bash
curl -X POST http://localhost:8000/workouts ^
  -H "Authorization: Bearer <TOKEN>"
```

## Adicionar set a um workout (user)
```bash
curl -X POST http://localhost:8000/workouts/1/sets ^
  -H "Authorization: Bearer <TOKEN>" ^
  -H "Content-Type: application/json" ^
  -d "{ \"exercise_id\": 1, \"reps\": 8, \"weight\": 80 }"
```

## Admin: listar users
```bash
curl http://localhost:8000/admin/users ^
  -H "Authorization: Bearer <ADMIN_TOKEN>"
```

---

## Notas de segurança
- Passwords são guardadas com PBKDF2-HMAC-SHA256
- Sessões têm expiração (ex.: 7 dias)
- Endpoints admin validam role=admin

---

