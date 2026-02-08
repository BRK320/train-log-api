# Trainlog-api (C + Mongoose + SQLite)

REST API built in **C** with **Mongoose** (HTTP server) and **SQLite** (persistence).
Includes authentication with **Bearer token**, sessions, and role-based permissions (`admin` / `client`).

## Stack
- C (GCC / MinGW)
- Mongoose (HTTP)
- SQLite3
- Windows CNG (`bcrypt`) for RNG and hashing (PBKDF2)

---

## Features

- Authentication:
  - `POST /signup` (creates account + auto-login)
  - `POST /login`
  - `POST /logout`
  - `GET /me`
- Exercises:
  - `GET /exercises` (public)
  - `POST /exercises` (admin)
  - `PUT /exercises/:id` (admin)
  - `DELETE /exercises/:id` (admin)
  - `GET /exercises/:id` (public)
- Workouts (per user):
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

## Authentication and Permissions

### Bearer Token
- After login/signup you receive a `token`
- Send it in all protected routes:
  ```
  Authorization: Bearer <TOKEN>
  ```

### Roles
- `client`: access to own workouts + stats endpoints
- `admin`: can manage users and perform CRUD operations on exercises

---

## How to Compile (Windows + MSYS2 / MinGW)

Example (PowerShell):
```powershell
gcc (Get-ChildItem src\*.c) -o api.exe -lws2_32 -lsqlite3 -lbcrypt
```

---

## How to Run

```bash
.\api.exe
```

#### The API runs at:
- http://localhost:8000

#### The SQLite database is created at:
- db/gym.db
---

# Administrator Credentials (Default)
admin@local:admin

---

# Examples (curl)

## Health Check
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

## GET exercises (public)
```bash
curl http://localhost:8000/exercises
```

## Create exercise (admin)
```bash
curl -X POST http://localhost:8000/exercises ^
  -H "Authorization: Bearer <ADMIN_TOKEN>" ^
  -H "Content-Type: application/json" ^
  -d "{ \"name\":\"Bench Press\" }"
```

## Create workout (user)
```bash
curl -X POST http://localhost:8000/workouts ^
  -H "Authorization: Bearer <TOKEN>"
```

## Add set to a workout (user)
```bash
curl -X POST http://localhost:8000/workouts/1/sets ^
  -H "Authorization: Bearer <TOKEN>" ^
  -H "Content-Type: application/json" ^
  -d "{ \"exercise_id\": 1, \"reps\": 8, \"weight\": 80 }"
```

## Admin: list users
```bash
curl http://localhost:8000/admin/users ^
  -H "Authorization: Bearer <ADMIN_TOKEN>"
```

---

## Security Notes

- Passwords are stored using PBKDF2-HMAC-SHA256
- Sessions have expiration (e.g., 7 days)
- Admin endpoints validate role=admin

---
