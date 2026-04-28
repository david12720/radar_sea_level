#include "radar_server.h"
#include "coord_convert.h"
#include "earth_model.h"
#include "vendor/httplib.h"
#include "vendor/json.hpp"
#include <cstring>
#include <iostream>

using json = nlohmann::json;

RadarServer::RadarServer(QueryHandler& handler, int port)
    : handler_(handler), port_(port) {}

void RadarServer::start()
{
    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ── GET /radar ────────────────────────────────────────────────────────────
    svr.Get("/radar", [this](const httplib::Request&, httplib::Response& res) {
        if (!handler_.radarSet()) {
            res.status = 404;
            res.set_content(R"({"error":"radar position not set — call POST /radar first"})",
                            "application/json");
            return;
        }
        const LLA& r      = handler_.radar();
        double terrain_msl = handler_.radarTerrainMsl();
        LutMetadata meta   = handler_.lutMetadata();
        json out;
        out["lat_deg"]       = r.lat_deg;
        out["lon_deg"]       = r.lon_deg;
        out["alt_m"]         = r.alt_m;
        out["terrain_msl_m"] = terrain_msl;
        out["agl_m"]         = r.alt_m - terrain_msl;
        out["max_range_m"]   = handler_.maxRange();
        out["lut"]["az_count"]    = meta.az_count;
        out["lut"]["range_count"] = meta.range_count;
        out["lut"]["az_step_deg"] = meta.az_step_deg;
        out["lut"]["range_step_m"]= meta.range_step_m;
        res.set_content(out.dump(), "application/json");
    });

    // ── POST /radar ───────────────────────────────────────────────────────────
    svr.Post("/radar", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: invalid JSON"})", "application/json");
            return;
        }
        if (!body.contains("lat_deg") || !body.contains("lon_deg") || !body.contains("agl_m")) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: required fields: lat_deg, lon_deg, agl_m"})",
                            "application/json");
            return;
        }
        double lat, lon, alt;
        try {
            lat = body.at("lat_deg").get<double>();
            lon = body.at("lon_deg").get<double>();
            alt = body.at("agl_m").get<double>();
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: fields must be numbers"})", "application/json");
            return;
        }
        bool ok = handler_.setRadar(lat, lon, alt);
        if (!ok) {
            res.status = 422;
            res.set_content(R"({"error":"no DEM tiles found for that position — check tiles directory"})",
                            "application/json");
            return;
        }
        const LLA& r       = handler_.radar();
        double terrain_msl  = handler_.radarTerrainMsl();
        LutMetadata meta    = handler_.lutMetadata();
        json out;
        out["lat_deg"]       = r.lat_deg;
        out["lon_deg"]       = r.lon_deg;
        out["alt_m"]         = r.alt_m;
        out["terrain_msl_m"] = terrain_msl;
        out["agl_m"]         = r.alt_m - terrain_msl;
        out["max_range_m"]   = handler_.maxRange();
        out["lut"]["az_count"]     = meta.az_count;
        out["lut"]["range_count"]  = meta.range_count;
        out["lut"]["az_step_deg"]  = meta.az_step_deg;
        out["lut"]["range_step_m"] = meta.range_step_m;
        res.set_content(out.dump(), "application/json");
    });

    // ── GET /lut ──────────────────────────────────────────────────────────────
    // Returns raw binary: uint32 az_count, uint32 range_count, int32[az*range]
    // Layout: cells[az_idx * range_count + range_idx], little-endian.
    svr.Get("/lut", [this](const httplib::Request&, httplib::Response& res) {
        if (!handler_.radarSet()) {
            res.status = 404;
            res.set_content(R"({"error":"radar not set — call POST /radar first"})",
                            "application/json");
            return;
        }
        const int32_t* cells = handler_.lutCells();
        size_t cells_count   = handler_.lutCellsSize();
        LutMetadata meta     = handler_.lutMetadata();

        size_t body_size = 2 * sizeof(uint32_t) + cells_count * sizeof(int32_t);
        std::string body(body_size, '\0');

        uint32_t az    = meta.az_count;
        uint32_t range = meta.range_count;
        std::memcpy(body.data(),     &az,          sizeof(az));
        std::memcpy(body.data() + 4, &range,       sizeof(range));
        std::memcpy(body.data() + 8, cells,        cells_count * sizeof(int32_t));

        res.set_content(body, "application/octet-stream");
    });

    // ── GET /elevation ────────────────────────────────────────────────────────
    svr.Get("/elevation", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("lat") || !req.has_param("lon")) {
            res.status = 400;
            res.set_content(R"({"error":"missing required params: lat, lon"})", "application/json");
            return;
        }
        try {
            double lat  = std::stod(req.get_param_value("lat"));
            double lon  = std::stod(req.get_param_value("lon"));
            double elev = handler_.getElevation(lat, lon);
            res.set_content(json{{"elev_m", elev}}.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"lat and lon must be valid numbers"})", "application/json");
        }
    });

    // ── POST /query ───────────────────────────────────────────────────────────
    svr.Post("/query", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: invalid JSON"})", "application/json");
            return;
        }

        if (!body.contains("range_m") || !body.contains("azimuth_deg") ||
            !body.contains("elevation_deg") || !body.contains("terrain_msl_m")) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: required fields: range_m, azimuth_deg, elevation_deg, terrain_msl_m"})",
                            "application/json");
            return;
        }

        RadarQuery q;
        try {
            q.range_m            = body.at("range_m").get<double>();
            q.azimuth_deg        = body.at("azimuth_deg").get<double>();
            q.elevation_deg      = body.at("elevation_deg").get<double>();
            q.terrain_msl_m      = body.at("terrain_msl_m").get<double>();
            if (body.contains("earth_model"))
                q.earth_model = body.at("earth_model").get<std::string>();
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: fields must be numbers"})", "application/json");
            return;
        }

        try {
            TargetResult r = handler_.handle(q);
            json out;
            out["lat_deg"]           = r.position.lat_deg;
            out["lon_deg"]           = r.position.lon_deg;
            out["alt_msl_m"]         = r.position.alt_m;
            out["terrain_msl_m"]     = r.terrain_msl_m;
            out["agl_m"]             = r.target_height_agl_m;
            out["horiz_range_m"]     = r.horizontal_range_m;
            out["relative_elev_deg"] = r.relative_elevation_deg;
            out["earth_model"]       = q.earth_model.empty() ? "flat" : q.earth_model;
            res.set_content(out.dump(), "application/json");
        } catch (const RadarNotSetError& e) {
            res.status = 503;
            res.set_content(json{{"error", std::string(e.what())}}.dump(), "application/json");
        } catch (const ValidationError& e) {
            res.status = 400;
            res.set_content(json{{"error", std::string("validation failed: ") + e.what()}}.dump(),
                            "application/json");
        } catch (const NoCoverageError& e) {
            res.status = 422;
            res.set_content(json{{"error", std::string("no DEM coverage at target location: ") + e.what()}}.dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", "internal server error"}}.dump(), "application/json");
            std::cerr << "[server] internal error: " << e.what() << "\n";
        }
    });

    // ── POST /convert ─────────────────────────────────────────────────────────
    svr.Post("/convert", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: invalid JSON"})", "application/json");
            return;
        }
        if (!body.contains("direction")) {
            res.status = 400;
            res.set_content(R"({"error":"missing field: direction"})", "application/json");
            return;
        }
        std::string dir = body.at("direction").get<std::string>();
        try {
            if (dir == "ll_to_utm") {
                if (!body.contains("lat_deg") || !body.contains("lon_deg")) {
                    res.status = 400;
                    res.set_content(R"({"error":"ll_to_utm requires lat_deg and lon_deg"})", "application/json");
                    return;
                }
                double lat = body.at("lat_deg").get<double>();
                double lon = body.at("lon_deg").get<double>();
                UtmPoint u = ll_to_utm(lat, lon);
                json out;
                out["easting"]    = u.easting;
                out["northing"]   = u.northing;
                out["zone"]       = u.zone;
                out["hemisphere"] = std::string(1, u.hemisphere);
                res.set_content(out.dump(), "application/json");
            } else if (dir == "utm_to_ll") {
                if (!body.contains("easting") || !body.contains("northing") ||
                    !body.contains("zone")     || !body.contains("hemisphere")) {
                    res.status = 400;
                    res.set_content(R"({"error":"utm_to_ll requires easting, northing, zone, hemisphere"})",
                                    "application/json");
                    return;
                }
                UtmPoint u;
                u.easting    = body.at("easting").get<double>();
                u.northing   = body.at("northing").get<double>();
                u.zone       = body.at("zone").get<int>();
                std::string hem = body.at("hemisphere").get<std::string>();
                if (hem.empty()) {
                    res.status = 400;
                    res.set_content(R"({"error":"hemisphere must be 'N' or 'S'"})", "application/json");
                    return;
                }
                u.hemisphere = static_cast<char>(hem[0]);
                double lat, lon;
                utm_to_ll(u, lat, lon);
                json out;
                out["lat_deg"] = lat;
                out["lon_deg"] = lon;
                res.set_content(out.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(R"({"error":"direction must be 'll_to_utm' or 'utm_to_ll'"})",
                                "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", std::string("bad request: ") + e.what()}}.dump(),
                            "application/json");
        }
    });

    std::cout << "[server] Listening on port " << port_ << " -- Ctrl-C to stop\n";
    std::cout.flush();
    svr.listen("0.0.0.0", port_);
}
