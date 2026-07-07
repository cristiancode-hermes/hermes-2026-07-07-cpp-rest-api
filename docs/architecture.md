# Arquitectura — C++ REST API

## Visión general

Servidor RESTful monolítico en C++20 con SQLite como almacenamiento. Diseñado para simplicidad y mínimo acoplamiento externo: todas las dependencias importantes son header-only (cpp-httplib, nlohmann/json) o se resuelven por FetchContent.

## Diagrama de flujo

```
┌──────────┐     HTTP/1.1     ┌───────────────────┐     SQL      ┌─────────┐
│  Cliente  │ ──────────────> │  cpp-httplib       │ ──────────> │ SQLite  │
│  curl /   │ <────────────── │  (Server + Routes)  │ <────────── │  (WAL)  │
│  fetch    │    JSON         │                     │   rows      │         │
└──────────┘                 └───────────────────┘             └─────────┘
                                     │
                                     │ spdlog
                                     ▼
                              ┌──────────────┐
                              │   stdout /    │
                              │   stderr      │
                              └──────────────┘
```

## Componentes

### 1. Database (`Database` class)
- RAII wrapper sobre `sqlite3*`
- Métodos: `query()` (SELECT → vector<json>), `execute()` (INSERT/UPDATE/DELETE → affected rows), `insert()` (→ last_insert_rowid)
- WAL mode activado en constructor
- Manejo de errores vía `std::expected<T, Error>`

### 2. Server (`Server` class)
- Envuelve `httplib::Server`
- `setup_routes()` configura handlers con CORS
- Handlers separados por método/recurso
- Paginación: `page` + `per_page` query params
- Búsqueda: `search` query param (escape básico de SQLite)
- Parseo de JSON con validación

### 3. Error handling
```cpp
struct Error { int code; std::string message; };
template <typename T> using Result = std::expected<T, Error>;
```
Todos los errores de DB se convierten a respuestas JSON con `error: true`.

## Decisiones técnicas

| Decisión | Razón |
|----------|-------|
| **cpp-httplib** (header-only) | 0 dependencias compiladas, integración trivial con FetchContent |
| **SQLite WAL** | Lecturas concurrentes sin bloqueos, ideal para API REST |
| **std::expected** | Manejo de errores funcional sin excepciones en hot path |
| **FetchContent** en vez de vcpkg | Reproducible sin instalación previa de paquetes |
| **Catch2 v3** | Única cabecera para testear, benchmarks incluidos |

## Endpoints y flujos

### POST /api/items → 201
```
Request:  {"name": "...", "description": "..."}
Validar → escape SQL → INSERT → SELECT → Response 201
Error: 400 (invalid JSON / missing fields), 500 (DB error)
```

### GET /api/items → 200
```
Query params: page, per_page, search
COUNT(*) → SELECT con LIMIT/OFFSET → Response con paginación
```

## Seguridad

- Escape de comillas simples en inputs de SQL (mitigación básica de SQLi)
- Sin secretos en el código fuente
- Docker corre como usuario no-root
