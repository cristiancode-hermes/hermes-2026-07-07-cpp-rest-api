# C++ REST API Server

> **API RESTful moderna escrita en C++20 con SQLite, lista para producción en Docker.**

---

## Features

- ✅ **CRUD completo** de items con paginación y búsqueda
- ✅ **SQLite con WAL mode** — concurrencia sin bloqueos
- ✅ **CORS** habilitado para desarrollo frontend
- ✅ **Sanitizers** (AddressSanitizer + UndefinedBehaviorSanitizer) en debug
- ✅ **Docker multi-stage** — imagen final de ~25 MB
- ✅ **Tests de integración** con Catch2 (incluye benchmarks)
- ✅ **CMake presets** — debug (asan+ubsan) y release (LTO)
- ✅ **Logging estructurado** con spdlog

## Endpoints

| Método | Ruta              | Descripción                     |
|--------|-------------------|---------------------------------|
| GET    | `/api/health`     | Health check + versión          |
| GET    | `/api/items`      | Listar items (paginado+search)  |
| GET    | `/api/items/:id`  | Obtener item por ID             |
| POST   | `/api/items`      | Crear item                      |
| PUT    | `/api/items/:id`  | Actualizar item                 |
| DELETE | `/api/items/:id`  | Eliminar item                   |

### Paginación

```
GET /api/items?page=1&per_page=20&search=keyword
```

Respuesta:
```json
{
  "data": [...],
  "page": 1,
  "per_page": 20,
  "total": 42,
  "total_pages": 3
}
```

## Requisitos

- **Compilador**: GCC 13+ o Clang 16+
- **CMake** ≥ 3.28
- **Ninja** (recomendado)
- **SQLite3** (development headers: `libsqlite3-dev`)
- **Git**

## Compilar y ejecutar

```bash
# Configurar (debug con sanitizers)
cmake --preset debug

# Compilar
cmake --build --preset debug

# Ejecutar
./build/debug/cpp-rest-api --port 8080 --db data/items.db
```

### Release

```bash
cmake --preset release && cmake --build --preset release
./build/release/cpp-rest-api
```

## Tests

```bash
# Los tests requieren el servidor corriendo
./build/debug/cpp-rest-api --port 8080 --db /tmp/test.db &

# En otra terminal:
cd build/debug && ctest --preset debug --output-on-failure
```

## Docker

```bash
# Construir
docker build -t cpp-rest-api .

# Ejecutar
docker run -p 8080:8080 -v "$(pwd)/data:/data" cpp-rest-api

# Probar
curl http://localhost:8080/api/health
```

## Ejemplo de uso

```bash
# Crear item
curl -X POST http://localhost:8080/api/items \
  -H "Content-Type: application/json" \
  -d '{"name": "Ejemplo", "description": "Ítem de prueba"}'

# Listar items
curl http://localhost:8080/api/items

# Buscar
curl "http://localhost:8080/api/items?search=Ejemplo"
```

## Stack técnico

| Componente      | Librería                                   |
|-----------------|---------------------------------------------|
| HTTP Server     | [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only) |
| JSON            | [nlohmann/json](https://github.com/nlohmann/json) v3.11+            |
| Logging         | [spdlog](https://github.com/gabime/spdlog) v1.15+                   |
| Base de datos   | SQLite3 v3.45+                           |
| Testing         | [Catch2](https://github.com/catchorg/Catch2) v3.8+                  |
| Build           | CMake 3.28+ / Ninja / vcpkg |

## Estructura del proyecto

```
├── CMakeLists.txt          # Build principal
├── CMakePresets.json       # Debug / Release presets
├── Dockerfile              # Multi-stage build
├── vcpkg.json              # Dependencias (opcional)
├── .env.example            # Variables de entorno
├── src/
│   └── main.cpp            # Servidor + controladores + DB
├── include/api/
│   └── server.hpp          # Header público
├── tests/
│   ├── CMakeLists.txt      # Build de tests
│   └── test_main.cpp       # Tests unitarios + integración
└── docs/
    └── architecture.md     # Documentación técnica
```
