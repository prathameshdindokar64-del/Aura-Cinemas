// ============================================================================
// AuraCinemas — Booking API Controller
// REST endpoints for seat locking, booking, cancellation, and seat maps.
// ============================================================================
#pragma once

#include <crow.h>
#include <nlohmann/json.hpp>

#include "services/booking_service.hpp"

namespace aura {

using json = nlohmann::json;

inline void register_booking_routes(crow::SimpleApp& app,
                                     BookingService& booking) {

    // -----------------------------------------------------------------------
    // GET /api/shows/:show_id/seats — seat map for a show
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/shows/<int>/seats")
        .methods(crow::HTTPMethod::GET)
    ([&booking](int show_id) {
        auto seats = booking.get_seat_map(show_id);

        json rows = json::object();
        for (const auto& s : seats) {
            std::string label = s.row_label + "-" + std::to_string(s.seat_number);
            json seat_obj = {
                {"seat_id",      s.seat_id},
                {"show_seat_id", s.show_seat_id},
                {"row",          s.row_label},
                {"number",       s.seat_number},
                {"label",        label},
                {"category",     s.category},
                {"status",       s.status},
                {"base_price",   s.base_price}
            };
            rows[s.row_label].push_back(seat_obj);
        }

        json resp = {
            {"success", true},
            {"show_id", show_id},
            {"seat_map", rows},
            {"total_seats", seats.size()}
        };
        return crow::response(200, resp.dump());
    });

    // -----------------------------------------------------------------------
    // POST /api/shows/:show_id/lock — lock seats for a user
    // Body: { "user_id": 1, "seat_ids": [10, 11, 12] }
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/shows/<int>/lock")
        .methods(crow::HTTPMethod::POST)
    ([&booking](const crow::request& req, int show_id) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("user_id") || !body.contains("seat_ids")) {
            json err = {{"success", false}, {"error", "Missing user_id or seat_ids"}};
            return crow::response(400, err.dump());
        }

        int user_id = body["user_id"].get<int>();
        auto seat_ids = body["seat_ids"].get<std::vector<int>>();

        auto result = booking.lock_seats(user_id, show_id, seat_ids);

        if (!result.ok()) {
            json err = {{"success", false}, {"error", result.message}};
            int status = (result.error == BookingError::SeatsUnavailable) ? 409 : 500;
            return crow::response(status, err.dump());
        }

        json resp = {
            {"success", true},
            {"message", result.message},
            {"lock_duration_seconds", BookingService::LOCK_DURATION_SECONDS}
        };
        return crow::response(200, resp.dump());
    });

    // -----------------------------------------------------------------------
    // POST /api/bookings — confirm a booking
    // Body: { "user_id", "show_id", "seats": [...], "payment_method", "payment_gateway" }
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/bookings")
        .methods(crow::HTTPMethod::POST)
    ([&booking](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            json err = {{"success", false}, {"error", "Invalid JSON body"}};
            return crow::response(400, err.dump());
        }

        BookingRequest br;
        br.user_id         = body.value("user_id", 0);
        br.show_id         = body.value("show_id", 0);
        br.payment_method  = body.value("payment_method", "UPI");
        br.payment_gateway = body.value("payment_gateway", "Razorpay");

        if (body.contains("seats") && body["seats"].is_array()) {
            for (const auto& s : body["seats"]) {
                SeatSelection sel;
                sel.seat_id       = s.value("seat_id", 0);
                sel.base_price    = s.value("base_price", 0.0);
                sel.seat_label    = s.value("seat_label", "");
                sel.category_name = s.value("category_name", "Standard");
                br.seats.push_back(sel);
            }
        }

        auto result = booking.confirm_booking(br);

        if (!result.ok()) {
            json err = {{"success", false}, {"error", result.message}};
            int status = (result.error == BookingError::LockExpired) ? 410 : 500;
            return crow::response(status, err.dump());
        }

        const auto& r = *result.receipt;
        json resp = {
            {"success", true},
            {"message", result.message},
            {"booking", {
                {"booking_id",      r.booking_id},
                {"payment_id",      r.payment_id},
                {"transaction_ref", r.transaction_ref},
                {"base_total",      r.base_total},
                {"surge_total",     r.surge_total},
                {"tax_amount",      r.tax_amount},
                {"convenience_fee", r.convenience_fee},
                {"grand_total",     r.grand_total},
                {"status",          r.status}
            }}
        };
        return crow::response(201, resp.dump());
    });

    // -----------------------------------------------------------------------
    // DELETE /api/bookings/:booking_id — cancel a booking
    // Body: { "user_id": 1 }
    // -----------------------------------------------------------------------
    CROW_ROUTE(app, "/api/bookings/<int>")
        .methods(crow::HTTPMethod::DELETE)
    ([&booking](const crow::request& req, int booking_id) {
        auto body = json::parse(req.body, nullptr, false);
        int user_id = body.is_discarded() ? 0 : body.value("user_id", 0);

        if (user_id <= 0) {
            json err = {{"success", false}, {"error", "Missing user_id"}};
            return crow::response(400, err.dump());
        }

        auto result = booking.cancel_booking(booking_id, user_id);

        if (!result.ok()) {
            json err = {{"success", false}, {"error", result.message}};
            return crow::response(500, err.dump());
        }

        json resp = {{"success", true}, {"message", result.message}};
        return crow::response(200, resp.dump());
    });
}

} // namespace aura
