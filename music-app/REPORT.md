# Favorite Music Management System: Project Report

Date: 21 November 2026

## 1. Overview

A web-based application for managing a personal collection of favorite songs. Users can add, view, search, update and delete songs from a browser. The backend is written in object-oriented C++, the data is stored in PostgreSQL, and the interface imitates the look of a classic Windows Forms application of the Visual Studio 2010 era (grey buttons, group boxes, a data grid with blue row selection, a status bar).

## 2. Objectives

- Build the system using object-oriented C++.
- Store data in a PostgreSQL database.
- Provide a graphical interface that can be reached over the internet.
- Keep the design layered, so the storage or the interface can be replaced without rewriting the rest.

## 3. Technology

| Part | Choice |
|---|---|
| Language | C++17 |
| Web server | cpp-httplib (single header file `httplib.h`) |
| JSON | nlohmann/json |
| Database | PostgreSQL, accessed through libpq |
| Front end | One HTML file with CSS and plain JavaScript |

## 4. Architecture

The program is split into five classes, each with one job.

| Class | Responsibility |
|---|---|
| `Song` | Holds the data of one song (id, title, artist, album, genre, year, rating). |
| `IMusicRepository` | Abstract interface listing what any storage must do: add, update, remove, getAll, search. |
| `PostgresRepository` | Implements the interface using PostgreSQL. |
| `MusicManager` | Business rules: validates a song before it is saved. |
| `ApiServer` | Receives HTTP requests, calls the manager and returns JSON. Also serves the web page. |

Flow of a request:

Browser -> `ApiServer` -> `MusicManager` -> `IMusicRepository` (`PostgresRepository`) -> PostgreSQL

### OOP concepts used

- **Encapsulation:** `Song` keeps its fields private and exposes getters and setters.
- **Abstraction and polymorphism:** `MusicManager` depends on `IMusicRepository`, not on PostgreSQL. A different storage class (for example an in-memory one for testing) can be passed in instead.
- **Single responsibility:** validation, storage and HTTP handling live in separate classes.
- **RAII:** database results are wrapped in `unique_ptr` and the connection is closed in the destructor.

## 5. Database design

Table `songs`:

| Column | Type | Rules |
|---|---|---|
| id | INTEGER, identity | Primary key, generated automatically |
| title | VARCHAR(200) | Required |
| artist | VARCHAR(200) | Required |
| album | VARCHAR(200) | Default empty |
| genre | VARCHAR(100) | Default empty |
| release_year | INTEGER | Required, between 1900 and 2100 |
| rating | INTEGER | Required, between 1 and 5 |
| created_at | TIMESTAMPTZ | Defaults to the current time |

An index on `artist` speeds up searching by artist. The same rules (year and rating ranges) are enforced twice: by `MusicManager` for friendly error messages, and by `CHECK` constraints in the database as a safety net.

## 6. API

| Method and path | Purpose | Success |
|---|---|---|
| GET `/api/songs` | List all songs | 200 |
| GET `/api/songs?q=text` | Search title, artist, album and genre (case-insensitive) | 200 |
| POST `/api/songs` | Add a song (JSON body) | 201 |
| PUT `/api/songs/{id}` | Update a song | 200 |
| DELETE `/api/songs/{id}` | Delete a song | 200 |
| GET `/health` | Simple check that the server is up | 200 |

Errors return JSON like `{"error": "Title cannot be empty."}` with status 400 (invalid data), 404 (song not found) or 500 (server problem, details only in the server log).

## 7. User interface

One page with two group boxes. The left box ("Song Details") has the input fields and the Add, Update, Delete and Clear buttons. The right box ("Song List") has a search bar and a grid of songs. Clicking a row loads it into the form, enables Update and Delete, and disables Add. Delete asks for confirmation. Messages appear as pop-up alerts and in the status bar. On narrow phone screens the two boxes stack vertically.

## 8. Security measures

- All SQL uses parameters (`$1`, `$2`, ...), which prevents SQL injection.
- The page escapes text before showing it, which prevents injected HTML or scripts.
- The database password is read from the `DATABASE_URL` environment variable and is never stored in the code.
- A mutex protects the single database connection from concurrent web requests.

## 9. Setup on Ubuntu / Xubuntu

1. Install tools: `sudo apt install g++ libpq-dev nlohmann-json3-dev`
2. Download `httplib.h` from the cpp-httplib project (github.com/yhirose/cpp-httplib) into the same folder as `main.cpp`.
3. Create the database: `sudo -u postgres createdb musicdb`
4. Create the table: `sudo -u postgres psql -d musicdb -f schema.sql`
5. Set the connection link (format `postgresql://USER:PASSWORD@HOST:PORT/DBNAME`):
   `export DATABASE_URL="postgresql://postgres:YOURPASSWORD@localhost:5432/musicdb"`
6. Build: `g++ -std=c++17 main.cpp -o musicapp -lpq -pthread`
7. Run: `./musicapp` (optionally set `PORT`, default is 8080)
8. Open `http://localhost:8080`

Folder layout:

```
music-app/
  main.cpp
  httplib.h        (you download this)
  schema.sql
  REPORT.md
  public/
    index.html
```

## 10. Putting it on the internet

- **Temporary sharing:** run the app on your own computer and open a public link to it with a tunnel tool (Cloudflare Tunnel or ngrok). It only works while your computer and the app are running.
- **Permanent hosting:** rent a small Ubuntu server (VPS), repeat the setup steps above on it, and keep the program running with `systemd`. Use a hosted or local PostgreSQL there and set `DATABASE_URL` on that server. If the database is on a different machine, add `?sslmode=require` to the link.

## 11. Limitations

- There is no login. Anyone who can open the page can change the data. This must be fixed before the site is public.
- The code was written but not compiled or run in the environment where it was produced, so the first build may need small fixes.
- One shared database connection is used. This is fine for a personal app but would not scale to many users (a connection pool would be needed).
- There is no pagination; all songs are loaded at once.

## 12. Possible future work

- Password login (or a simple admin password) for adding, editing and deleting.
- Sorting by column, pagination, and a favorites or playlist table.
- Automated tests using an in-memory implementation of `IMusicRepository`.
- A connection pool and HTTPS (usually provided by a reverse proxy such as nginx).
