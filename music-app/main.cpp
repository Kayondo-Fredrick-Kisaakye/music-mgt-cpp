// Favorite Music Management System - web edition (OOP C++)
//
//   Song              -> the data (model)
//   IMusicRepository  -> contract for storing songs (interface)
//   PostgresRepository-> real database storage (libpq)
//   MusicManager      -> validation / business rules
//   ApiServer         -> HTTP + JSON API, also serves the ./public web page
//
// Configuration comes from environment variables (never hard-code passwords):
//   DATABASE_URL = postgresql://user:password@host:5432/dbname
//   PORT         = port to listen on (default 8080)

#include "httplib.h"
#include <nlohmann/json.hpp>
#include <libpq-fe.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

// ------------------------------------------------------------
// MODEL
// ------------------------------------------------------------
class Song {
private:
    int id;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int year;
    int rating;

public:
    Song() : id(0), year(0), rating(0) {}

    Song(int id, const std::string& title, const std::string& artist,
         const std::string& album, const std::string& genre,
         int year, int rating)
        : id(id), title(title), artist(artist), album(album),
          genre(genre), year(year), rating(rating) {}

    int getId() const { return id; }
    const std::string& getTitle() const { return title; }
    const std::string& getArtist() const { return artist; }
    const std::string& getAlbum() const { return album; }
    const std::string& getGenre() const { return genre; }
    int getYear() const { return year; }
    int getRating() const { return rating; }

    void setId(int v) { id = v; }
    void setTitle(const std::string& v) { title = v; }
    void setArtist(const std::string& v) { artist = v; }
    void setAlbum(const std::string& v) { album = v; }
    void setGenre(const std::string& v) { genre = v; }
    void setYear(int v) { year = v; }
    void setRating(int v) { rating = v; }
};

// ------------------------------------------------------------
// REPOSITORY INTERFACE
// ------------------------------------------------------------
class IMusicRepository {
public:
    virtual ~IMusicRepository() {}
    virtual int add(const Song& song) = 0;                 // returns new id
    virtual bool update(const Song& song) = 0;             // false = not found
    virtual bool remove(int id) = 0;                       // false = not found
    virtual std::vector<Song> getAll() const = 0;
    virtual std::vector<Song> search(const std::string& keyword) const = 0;
};

// ------------------------------------------------------------
// POSTGRESQL REPOSITORY (libpq)
// ------------------------------------------------------------
class PostgresRepository : public IMusicRepository {
private:
    struct ResultDeleter {
        void operator()(PGresult* r) const { PQclear(r); }
    };
    typedef std::unique_ptr<PGresult, ResultDeleter> ResultPtr;

    mutable PGconn* conn;
    mutable std::mutex mtx; // one connection, many web threads -> lock

    static const char* columns() {
        return "id, title, artist, album, genre, release_year, rating";
    }

    // Runs a parameterized query ($1, $2, ...). Parameters are sent
    // separately from the SQL, which protects against SQL injection.
    ResultPtr exec(const char* sql, const std::vector<std::string>& params) const {
        if (PQstatus(conn) != CONNECTION_OK) PQreset(conn);

        std::vector<const char*> values;
        for (size_t i = 0; i < params.size(); i++) values.push_back(params[i].c_str());

        PGresult* raw = PQexecParams(conn, sql, (int)values.size(), NULL,
                                     values.empty() ? NULL : &values[0],
                                     NULL, NULL, 0);
        ResultPtr res(raw);
        ExecStatusType st = PQresultStatus(raw);
        if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
            throw std::runtime_error(PQerrorMessage(conn));
        }
        return res;
    }

    static Song rowToSong(PGresult* r, int i) {
        return Song(std::atoi(PQgetvalue(r, i, 0)),
                    PQgetvalue(r, i, 1),
                    PQgetvalue(r, i, 2),
                    PQgetvalue(r, i, 3),
                    PQgetvalue(r, i, 4),
                    std::atoi(PQgetvalue(r, i, 5)),
                    std::atoi(PQgetvalue(r, i, 6)));
    }

    static std::vector<Song> rowsToSongs(PGresult* r) {
        std::vector<Song> list;
        int n = PQntuples(r);
        for (int i = 0; i < n; i++) list.push_back(rowToSong(r, i));
        return list;
    }

public:
    explicit PostgresRepository(const std::string& connectionString) {
        conn = PQconnectdb(connectionString.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string err = PQerrorMessage(conn);
            PQfinish(conn);
            throw std::runtime_error("Database connection failed: " + err);
        }
    }

    ~PostgresRepository() { PQfinish(conn); }

    PostgresRepository(const PostgresRepository&) = delete;
    PostgresRepository& operator=(const PostgresRepository&) = delete;

    int add(const Song& s) override {
        std::lock_guard<std::mutex> lock(mtx);
        ResultPtr r = exec(
            "INSERT INTO songs (title, artist, album, genre, release_year, rating) "
            "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
            { s.getTitle(), s.getArtist(), s.getAlbum(), s.getGenre(),
              std::to_string(s.getYear()), std::to_string(s.getRating()) });
        return std::atoi(PQgetvalue(r.get(), 0, 0));
    }

    bool update(const Song& s) override {
        std::lock_guard<std::mutex> lock(mtx);
        ResultPtr r = exec(
            "UPDATE songs SET title=$1, artist=$2, album=$3, genre=$4, "
            "release_year=$5, rating=$6 WHERE id=$7",
            { s.getTitle(), s.getArtist(), s.getAlbum(), s.getGenre(),
              std::to_string(s.getYear()), std::to_string(s.getRating()),
              std::to_string(s.getId()) });
        return std::atoi(PQcmdTuples(r.get())) > 0;
    }

    bool remove(int id) override {
        std::lock_guard<std::mutex> lock(mtx);
        ResultPtr r = exec("DELETE FROM songs WHERE id=$1", { std::to_string(id) });
        return std::atoi(PQcmdTuples(r.get())) > 0;
    }

    std::vector<Song> getAll() const override {
        std::lock_guard<std::mutex> lock(mtx);
        std::string sql = std::string("SELECT ") + columns() + " FROM songs ORDER BY id";
        ResultPtr r = exec(sql.c_str(), {});
        return rowsToSongs(r.get());
    }

    std::vector<Song> search(const std::string& keyword) const override {
        std::lock_guard<std::mutex> lock(mtx);
        std::string sql = std::string("SELECT ") + columns() +
            " FROM songs WHERE title ILIKE $1 OR artist ILIKE $1 "
            "OR album ILIKE $1 OR genre ILIKE $1 ORDER BY id";
        ResultPtr r = exec(sql.c_str(), { "%" + keyword + "%" });
        return rowsToSongs(r.get());
    }
};

// ------------------------------------------------------------
// MANAGER (business rules)
// ------------------------------------------------------------
class MusicManager {
private:
    IMusicRepository& repo;

    void validate(const Song& s) const {
        if (s.getTitle().empty())  throw std::invalid_argument("Title cannot be empty.");
        if (s.getArtist().empty()) throw std::invalid_argument("Artist cannot be empty.");
        if (s.getYear() < 1900 || s.getYear() > 2100)
            throw std::invalid_argument("Year must be between 1900 and 2100.");
        if (s.getRating() < 1 || s.getRating() > 5)
            throw std::invalid_argument("Rating must be between 1 and 5.");
    }

public:
    explicit MusicManager(IMusicRepository& repository) : repo(repository) {}

    int addSong(const Song& s) { validate(s); return repo.add(s); }
    bool updateSong(const Song& s) { validate(s); return repo.update(s); }
    bool deleteSong(int id) { return repo.remove(id); }
    std::vector<Song> getAllSongs() const { return repo.getAll(); }
    std::vector<Song> searchSongs(const std::string& k) const { return repo.search(k); }
};

// ------------------------------------------------------------
// API SERVER
// ------------------------------------------------------------
class ApiServer {
private:
    MusicManager& manager;
    httplib::Server svr;

    static json toJson(const Song& s) {
        json j;
        j["id"] = s.getId();
        j["title"] = s.getTitle();
        j["artist"] = s.getArtist();
        j["album"] = s.getAlbum();
        j["genre"] = s.getGenre();
        j["year"] = s.getYear();
        j["rating"] = s.getRating();
        return j;
    }

    static Song fromJson(const json& j) {
        Song s;
        s.setTitle(j.value("title", std::string()));
        s.setArtist(j.value("artist", std::string()));
        s.setAlbum(j.value("album", std::string()));
        s.setGenre(j.value("genre", std::string()));
        s.setYear(j.value("year", 0));
        s.setRating(j.value("rating", 0));
        return s;
    }

    static json errorJson(const std::string& message) {
        json j;
        j["error"] = message;
        return j;
    }

    static void reply(httplib::Response& res, int status, const json& body) {
        res.status = status;
        res.set_content(body.dump(), "application/json");
    }

    // Wraps every route so errors become clean JSON responses.
    template <typename F>
    static void guarded(httplib::Response& res, F fn) {
        try {
            fn();
        } catch (const json::exception&) {
            reply(res, 400, errorJson("Invalid data."));
        } catch (const std::invalid_argument& e) {
            reply(res, 400, errorJson(e.what()));
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
            reply(res, 500, errorJson("Server error. Check the server log."));
        }
    }

    void setupRoutes() {
        svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("ok", "text/plain");
        });

        svr.Get("/api/songs", [this](const httplib::Request& req, httplib::Response& res) {
            guarded(res, [&]() {
                std::string q = req.has_param("q") ? req.get_param_value("q") : "";
                std::vector<Song> songs = q.empty() ? manager.getAllSongs()
                                                    : manager.searchSongs(q);
                json arr = json::array();
                for (size_t i = 0; i < songs.size(); i++) arr.push_back(toJson(songs[i]));
                reply(res, 200, arr);
            });
        });

        svr.Post("/api/songs", [this](const httplib::Request& req, httplib::Response& res) {
            guarded(res, [&]() {
                Song s = fromJson(json::parse(req.body));
                int id = manager.addSong(s);
                json out;
                out["id"] = id;
                reply(res, 201, out);
            });
        });

        svr.Put(R"(/api/songs/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            guarded(res, [&]() {
                Song s = fromJson(json::parse(req.body));
                s.setId(std::atoi(req.matches[1].str().c_str()));
                if (!manager.updateSong(s)) {
                    reply(res, 404, errorJson("Song not found."));
                    return;
                }
                json out;
                out["updated"] = true;
                reply(res, 200, out);
            });
        });

        svr.Delete(R"(/api/songs/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            guarded(res, [&]() {
                int id = std::atoi(req.matches[1].str().c_str());
                if (!manager.deleteSong(id)) {
                    reply(res, 404, errorJson("Song not found."));
                    return;
                }
                json out;
                out["deleted"] = true;
                reply(res, 200, out);
            });
        });

        // the web page (public/index.html)
        if (!svr.set_mount_point("/", "./public")) {
            std::cerr << "Warning: ./public folder not found." << std::endl;
        }
    }

public:
    explicit ApiServer(MusicManager& m) : manager(m) { setupRoutes(); }

    void run(int port) {
        std::cout << "Listening on port " << port << std::endl;
        if (!svr.listen("0.0.0.0", port)) {
            throw std::runtime_error("Could not start the server on that port.");
        }
    }
};

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------
int main() {
    const char* dbUrl = std::getenv("DATABASE_URL");
    if (!dbUrl) {
        std::cerr << "DATABASE_URL is not set.\n"
                  << "Example: export DATABASE_URL="
                  << "\"postgresql://postgres:YOURPASSWORD@localhost:5432/musicdb\"\n";
        return 1;
    }
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 8080;

    try {
        PostgresRepository repo(dbUrl);
        MusicManager manager(repo);
        ApiServer server(manager);
        server.run(port);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
