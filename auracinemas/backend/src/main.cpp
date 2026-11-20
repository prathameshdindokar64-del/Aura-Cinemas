// ============================================================================
// AuraCinemas — Server Entry Point
// Initializes DB, loads pricing rules, registers API routes, starts Crow.
// ============================================================================

#include <crow.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "database/db_manager.hpp"
#include "services/pricing_engine.hpp"
#include "services/booking_service.hpp"
#include "controllers/booking_controller.hpp"
#include "controllers/pricing_controller.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::vector<aura::PricingRule> load_pricing_rules(aura::DbManager& db) {
    std::vector<aura::PricingRule> rules;
    db.query(
        "SELECT rule_id, rule_name, rule_type, condition_desc, "
        "modifier_percent, priority, is_active FROM pricing_rules "
        "WHERE is_active = 1 ORDER BY priority",
        [&](sqlite3_stmt* stmt) {
            aura::PricingRule r;
            r.rule_id          = sqlite3_column_int(stmt, 0);
            r.rule_name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            r.rule_type        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            r.condition_desc   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            r.modifier_percent = sqlite3_column_double(stmt, 4);
            r.priority         = sqlite3_column_int(stmt, 5);
            r.is_active        = sqlite3_column_int(stmt, 6) == 1;
            rules.push_back(r);
        }
    );
    std::cout << "[Init] Loaded " << rules.size() << " pricing rules.\n";
    return rules;
}

// Register data-query routes (movies, cities, theaters, shows)
static void register_data_routes(crow::SimpleApp& app, aura::DbManager& db) {

    // GET /api/movies — list all active movies
    CROW_ROUTE(app, "/api/movies")
        .methods(crow::HTTPMethod::GET)
    ([&db](const crow::request& req) {
        std::string lang = req.url_params.get("language") ? req.url_params.get("language") : "";
        std::string sql =
            "SELECT movie_id, title, language, genre, duration_mins, "
            "certification, release_date, synopsis, director, cast_list, "
            "rating, poster_url FROM movies WHERE is_active = 1";
        if (!lang.empty()) sql += " AND language = '" + lang + "'";
        sql += " ORDER BY release_date DESC";

        json movies = json::array();
        db.query(sql, [&](sqlite3_stmt* s) {
            json m;
            m["movie_id"]      = sqlite3_column_int(s, 0);
            m["title"]         = reinterpret_cast<const char*>(sqlite3_column_text(s, 1));
            m["language"]      = reinterpret_cast<const char*>(sqlite3_column_text(s, 2));
            m["genre"]         = reinterpret_cast<const char*>(sqlite3_column_text(s, 3));
            m["duration_mins"] = sqlite3_column_int(s, 4);
            m["certification"] = reinterpret_cast<const char*>(sqlite3_column_text(s, 5));
            m["release_date"]  = reinterpret_cast<const char*>(sqlite3_column_text(s, 6));
            auto syn = sqlite3_column_text(s, 7);
            m["synopsis"]      = syn ? reinterpret_cast<const char*>(syn) : "";
            auto dir = sqlite3_column_text(s, 8);
            m["director"]      = dir ? reinterpret_cast<const char*>(dir) : "";
            auto cast = sqlite3_column_text(s, 9);
            m["cast"]          = cast ? reinterpret_cast<const char*>(cast) : "";
            m["rating"]        = sqlite3_column_double(s, 10);
            auto poster = sqlite3_column_text(s, 11);
            m["poster_url"]    = poster ? reinterpret_cast<const char*>(poster) : "";
            movies.push_back(m);
        });
        json resp = {{"success", true}, {"count", movies.size()}, {"movies", movies}};
        return crow::response(200, resp.dump());
    });

    // GET /api/cities — list all cities
    CROW_ROUTE(app, "/api/cities")
        .methods(crow::HTTPMethod::GET)
    ([&db](const crow::request&) {
        json cities = json::array();
        db.query("SELECT city_id, city_name, state FROM cities ORDER BY city_name",
            [&](sqlite3_stmt* s) {
                cities.push_back({
                    {"city_id",   sqlite3_column_int(s, 0)},
                    {"city_name", reinterpret_cast<const char*>(sqlite3_column_text(s, 1))},
                    {"state",     reinterpret_cast<const char*>(sqlite3_column_text(s, 2))}
                });
            });
        json resp = {{"success", true}, {"cities", cities}};
        return crow::response(200, resp.dump());
    });

    // GET /api/cities/:city_id/theaters — theaters in a city
    CROW_ROUTE(app, "/api/cities/<int>/theaters")
        .methods(crow::HTTPMethod::GET)
    ([&db](int city_id) {
        json theaters = json::array();
        std::string sql =
            "SELECT theater_id, theater_name, address, total_screens "
            "FROM theaters WHERE city_id = " + std::to_string(city_id) +
            " AND is_active = 1 ORDER BY theater_name";
        db.query(sql, [&](sqlite3_stmt* s) {
            theaters.push_back({
                {"theater_id",    sqlite3_column_int(s, 0)},
                {"theater_name",  reinterpret_cast<const char*>(sqlite3_column_text(s, 1))},
                {"address",       reinterpret_cast<const char*>(sqlite3_column_text(s, 2))},
                {"total_screens", sqlite3_column_int(s, 3)}
            });
        });
        json resp = {{"success", true}, {"city_id", city_id}, {"theaters", theaters}};
        return crow::response(200, resp.dump());
    });

    // GET /api/movies/:movie_id/shows?city_id=X — shows for a movie in a city
    CROW_ROUTE(app, "/api/movies/<int>/shows")
        .methods(crow::HTTPMethod::GET)
    ([&db](const crow::request& req, int movie_id) {
        std::string city_filter = "";
        if (req.url_params.get("city_id")) {
            city_filter = " AND t.city_id = " + std::string(req.url_params.get("city_id"));
        }

        std::string sql =
            "SELECT sh.show_id, sh.show_date, sh.start_time, sh.end_time, "
            "sc.screen_name, sc.screen_type, t.theater_name, t.theater_id, "
            "c.city_name "
            "FROM shows sh "
            "JOIN screens sc ON sh.screen_id = sc.screen_id "
            "JOIN theaters t ON sc.theater_id = t.theater_id "
            "JOIN cities c ON t.city_id = c.city_id "
            "WHERE sh.movie_id = " + std::to_string(movie_id) +
            " AND sh.is_active = 1" + city_filter +
            " ORDER BY sh.show_date, sh.start_time";

        json shows = json::array();
        db.query(sql, [&](sqlite3_stmt* s) {
            shows.push_back({
                {"show_id",      sqlite3_column_int(s, 0)},
                {"show_date",    reinterpret_cast<const char*>(sqlite3_column_text(s, 1))},
                {"start_time",   reinterpret_cast<const char*>(sqlite3_column_text(s, 2))},
                {"end_time",     reinterpret_cast<const char*>(sqlite3_column_text(s, 3))},
                {"screen_name",  reinterpret_cast<const char*>(sqlite3_column_text(s, 4))},
                {"screen_type",  reinterpret_cast<const char*>(sqlite3_column_text(s, 5))},
                {"theater_name", reinterpret_cast<const char*>(sqlite3_column_text(s, 6))},
                {"theater_id",   sqlite3_column_int(s, 7)},
                {"city",         reinterpret_cast<const char*>(sqlite3_column_text(s, 8))}
            });
        });
        json resp = {{"success", true}, {"movie_id", movie_id}, {"shows", shows}};
        return crow::response(200, resp.dump());
    });

    // GET /health — health check
    CROW_ROUTE(app, "/health")([](const crow::request&) {
        json resp = {{"status", "ok"}, {"service", "AuraCinemas API"}, {"version", "1.0.0"}};
        return crow::response(200, resp.dump());
    });
}

// ---------------------------------------------------------------------------
// BookingService SQLite helper implementations
// (Defined here since they need the sqlite3 C API)
// ---------------------------------------------------------------------------
namespace aura {
bool BookingService::exec_sql(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[BookingSvc] SQL error: " << (err ? err : "unknown") << "\n";
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

bool BookingService::prepare_stmt(const std::string& sql, sqlite3_stmt** out) {
    return sqlite3_prepare_v2(db_, sql.c_str(), -1, out, nullptr) == SQLITE_OK;
}

bool BookingService::step_stmt(sqlite3_stmt* stmt) {
    return sqlite3_step(stmt) == SQLITE_ROW;
}

void BookingService::finalize_stmt(sqlite3_stmt* stmt) {
    sqlite3_finalize(stmt);
}

int64_t BookingService::last_insert_rowid() {
    return sqlite3_last_insert_rowid(db_);
}

int BookingService::col_int(sqlite3_stmt* s, int i) {
    return sqlite3_column_int(s, i);
}

double BookingService::col_double(sqlite3_stmt* s, int i) {
    return sqlite3_column_double(s, i);
}

std::string BookingService::col_text(sqlite3_stmt* s, int i) {
    auto t = sqlite3_column_text(s, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}
} // namespace aura

// ===========================================================================
// main
// ===========================================================================
int main(int argc, char* argv[]) {
    std::cout << R"(
     _                     ____ _
    / \  _   _ _ __ __ _  / ___(_)_ __   ___ _ __ ___   __ _ ___
   / _ \| | | | '__/ _` || |   | | '_ \ / _ \ '_ ` _ \ / _` / __|
  / ___ \ |_| | | | (_| || |___| | | | |  __/ | | | | | (_| \__ \
 /_/   \_\__,_|_|  \__,_| \____|_|_| |_|\___|_| |_| |_|\__,_|___/
                          API Server v1.0.0
    )" << "\n";

    // ---- Configuration ----
    std::string db_path = "auracinemas.db";
    int port = 8080;

    if (argc > 1) db_path = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);

    // ---- Resolve SQL file paths ----
    fs::path exe_dir = fs::path(argv[0]).parent_path();
    fs::path schema_path = exe_dir / ".." / "database" / "schema.sql";
    fs::path dataset_path = exe_dir / ".." / "database" / "dataset.sql";

    // Fallback: look relative to CWD
    if (!fs::exists(schema_path)) schema_path = "database/schema.sql";
    if (!fs::exists(dataset_path)) dataset_path = "database/dataset.sql";

    // ---- Initialize Database ----
    aura::DbManager db(db_path);
    if (!db.open()) {
        std::cerr << "[Fatal] Cannot open database.\n";
        return 1;
    }

    // Run schema + seed only if tables don't exist yet
    if (!db.table_exists("movies")) {
        std::cout << "[Init] First run — creating schema...\n";
        if (!db.execute_file(schema_path.string())) {
            std::cerr << "[Fatal] Schema init failed.\n";
            return 1;
        }
        if (fs::exists(dataset_path)) {
            std::cout << "[Init] Seeding data...\n";
            db.execute_file(dataset_path.string());
        }
    } else {
        std::cout << "[Init] Database already initialized.\n";
    }

    // ---- Initialize Services ----
    auto rules = load_pricing_rules(db);
    aura::PricingEngine pricing(std::move(rules));
    aura::BookingService booking(db.handle(), pricing);

    // ---- Background: expired lock cleanup (every 60s) ----
    std::atomic<bool> running{true};
    std::thread lock_cleaner([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            if (running.load()) {
                booking.release_expired_locks();
            }
        }
    });
    lock_cleaner.detach();

    // ---- Setup Crow App ----
    crow::SimpleApp app;

    // CORS middleware (allow frontend dev server)
    auto& cors_mw = app.get_middleware<crow::CORSHandler>();
    // If CORSHandler is not available in your Crow build, remove these lines:
    // cors_mw.global().origin("*").methods("GET"_method, "POST"_method, "DELETE"_method, "OPTIONS"_method);

    // Register all route groups
    register_data_routes(app, db);
    aura::register_booking_routes(app, booking);
    aura::register_pricing_routes(app, pricing, db);

    // ---- Start Server ----
    std::cout << "[Server] Starting on http://localhost:" << port << "\n";
    std::cout << "[Server] API endpoints:\n"
              << "  GET  /health\n"
              << "  GET  /api/movies\n"
              << "  GET  /api/cities\n"
              << "  GET  /api/cities/:id/theaters\n"
              << "  GET  /api/movies/:id/shows\n"
              << "  GET  /api/shows/:id/seats\n"
              << "  GET  /api/shows/:id/surge\n"
              << "  POST /api/shows/:id/lock\n"
              << "  POST /api/bookings\n"
              << "  DEL  /api/bookings/:id\n"
              << "  POST /api/pricing/calculate\n"
              << "  GET  /api/pricing/rules\n";

    app.port(port).multithreaded().run();

    running.store(false);
    return 0;
}
