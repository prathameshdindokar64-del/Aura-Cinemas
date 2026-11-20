// ============================================================================
// AuraCinemas — Pricing API Controller
// REST endpoints for dynamic price calculation and pricing rules.
// ============================================================================
#pragma once

#include <crow.h>
#include <nlohmann/json.hpp>

#include "database/db_manager.hpp"
#include "services/pricing_engine.hpp"

namespace aura {

using json = nlohmann::json;

inline void register_pricing_routes(crow::SimpleApp& app,
                                     PricingEngine& engine,
                                     DbManager& db) {

    // -----------------------------------------------------------------------
    // POST /api/pricing/calculate — get dynamic price for seats
    // Body: { "show_id": 1, "base_prices": [200, 300, 200] }
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/pricing/calculate")
        .methods(crow::HTTPMethod::POST)
    ([&engine, &db](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("show_id")) {
            json err = {{"success", false}, {"error", "Missing show_id"}};
            return crow::response(400, err.dump());
        }

        int show_id = body["show_id"].get<int>();

        // Build ShowContext from DB
        ShowContext ctx;
        std::string sql =
            "SELECT s.show_date, s.start_time, sc.total_capacity, "
            "COALESCE(SUM(CASE WHEN ss.status='booked' THEN 1 ELSE 0 END),0), "
            "COALESCE(SUM(CASE WHEN ss.status='locked' THEN 1 ELSE 0 END),0) "
            "FROM shows s "
            "JOIN screens sc ON s.screen_id = sc.screen_id "
            "LEFT JOIN show_seats ss ON s.show_id = ss.show_id "
            "WHERE s.show_id = " + std::to_string(show_id) +
            " GROUP BY s.show_id";

        bool found = false;
        db.query(sql, [&](sqlite3_stmt* stmt) {
            ctx.show_date      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            ctx.start_time     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            ctx.total_capacity = sqlite3_column_int(stmt, 2);
            ctx.booked_count   = sqlite3_column_int(stmt, 3);
            ctx.locked_count   = sqlite3_column_int(stmt, 4);
            found = true;
        });

        if (!found) {
            json err = {{"success", false}, {"error", "Show not found"}};
            return crow::response(404, err.dump());
        }

        // Single or bulk pricing
        if (body.contains("base_prices") && body["base_prices"].is_array()) {
            auto prices = body["base_prices"].get<std::vector<double>>();
            auto bulk = engine.calculate_bulk(prices, ctx);

            json seat_details = json::array();
            for (const auto& pr : bulk.per_seat) {
                seat_details.push_back({
                    {"base_price",       pr.base_price},
                    {"surge_multiplier", pr.surge_multiplier},
                    {"dynamic_price",    pr.dynamic_price},
                    {"tax",              pr.tax_amount},
                    {"convenience_fee",  pr.convenience_fee},
                    {"final_price",      pr.final_price},
                    {"applied_rules",    pr.applied_rules}
                });
            }

            json resp = {
                {"success", true},
                {"show_id", show_id},
                {"occupancy_percent", ctx.occupancy_percent()},
                {"totals", {
                    {"base_total",       bulk.total_base},
                    {"surge_total",      bulk.total_surge},
                    {"tax_total",        bulk.total_tax},
                    {"convenience_total",bulk.total_convenience},
                    {"grand_total",      bulk.grand_total},
                    {"avg_multiplier",   bulk.avg_multiplier}
                }},
                {"seats", seat_details}
            };
            return crow::response(200, resp.dump());

        } else {
            double bp = body.value("base_price", 200.0);
            auto pr = engine.calculate_price(bp, ctx);

            json resp = {
                {"success", true},
                {"show_id", show_id},
                {"occupancy_percent", ctx.occupancy_percent()},
                {"pricing", {
                    {"base_price",       pr.base_price},
                    {"surge_multiplier", pr.surge_multiplier},
                    {"dynamic_price",    pr.dynamic_price},
                    {"tax",              pr.tax_amount},
                    {"convenience_fee",  pr.convenience_fee},
                    {"final_price",      pr.final_price},
                    {"applied_rules",    pr.applied_rules}
                }}
            };
            return crow::response(200, resp.dump());
        }
    });

    // -----------------------------------------------------------------------
    // GET /api/pricing/rules — list all active pricing rules
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/pricing/rules")
        .methods(crow::HTTPMethod::GET)
    ([&db](const crow::request&) {
        json rules = json::array();
        db.query(
            "SELECT rule_id, rule_name, rule_type, condition_desc, "
            "modifier_percent, priority, is_active FROM pricing_rules "
            "ORDER BY priority",
            [&](sqlite3_stmt* stmt) {
                rules.push_back({
                    {"rule_id",          sqlite3_column_int(stmt, 0)},
                    {"rule_name",        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))},
                    {"rule_type",        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))},
                    {"condition",        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))},
                    {"modifier_percent", sqlite3_column_double(stmt, 4)},
                    {"priority",         sqlite3_column_int(stmt, 5)},
                    {"is_active",        sqlite3_column_int(stmt, 6) == 1}
                });
            }
        );
        json resp = {{"success", true}, {"rules", rules}};
        return crow::response(200, resp.dump());
    });

    // -----------------------------------------------------------------------
    // GET /api/shows/:show_id/surge — real-time surge info for a show
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/shows/<int>/surge")
        .methods(crow::HTTPMethod::GET)
    ([&db](int show_id) {
        json data = json::object();
        std::string sql =
            "SELECT total_capacity, booked_count, locked_count, "
            "occupancy_percent, surge_multiplier, avg_dynamic_price, "
            "requests_per_min, snapshot_time "
            "FROM surge_analytics WHERE show_id = " +
            std::to_string(show_id) +
            " ORDER BY snapshot_time DESC LIMIT 1";

        bool found = false;
        db.query(sql, [&](sqlite3_stmt* stmt) {
            data = {
                {"show_id",           show_id},
                {"total_capacity",    sqlite3_column_int(stmt, 0)},
                {"booked_count",      sqlite3_column_int(stmt, 1)},
                {"locked_count",      sqlite3_column_int(stmt, 2)},
                {"occupancy_percent", sqlite3_column_double(stmt, 3)},
                {"surge_multiplier",  sqlite3_column_double(stmt, 4)},
                {"avg_dynamic_price", sqlite3_column_double(stmt, 5)},
                {"requests_per_min",  sqlite3_column_int(stmt, 6)},
                {"snapshot_time",     reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))}
            };
            found = true;
        });

        if (!found) {
            json err = {{"success", false}, {"error", "No surge data for this show"}};
            return crow::response(404, err.dump());
        }

        json resp = {{"success", true}, {"surge", data}};
        return crow::response(200, resp.dump());
    });
}

} // namespace aura
