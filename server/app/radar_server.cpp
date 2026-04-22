#include "radar_server.h"
#include "vendor/httplib.h"
#include "vendor/json.hpp"
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

    svr.Get("/radar", [this](const httplib::Request&, httplib::Response& res) {
        const LLA& r = handler_.radar();
        double ground_elev = handler_.getElevation(r.lat_deg, r.lon_deg);
        json out;
        out["lat_deg"]      = r.lat_deg;
        out["lon_deg"]      = r.lon_deg;
        out["alt_m"]        = r.alt_m;
        out["ground_elev_m"]= ground_elev;
        out["agl_m"]        = r.alt_m - ground_elev;
        out["max_range_m"]  = handler_.config().max_range_m;
        res.set_content(out.dump(), "application/json");
    });

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

    svr.Post("/query", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: invalid JSON"})", "application/json");
            return;
        }

        if (!body.contains("range_m") || !body.contains("azimuth_deg") || !body.contains("elevation_deg")) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: missing required fields"})", "application/json");
            return;
        }

        RadarQuery q;
        try {
            q.range_m       = body.at("range_m").get<double>();
            q.azimuth_deg   = body.at("azimuth_deg").get<double>();
            q.elevation_deg = body.at("elevation_deg").get<double>();
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad request: fields must be numbers"})", "application/json");
            return;
        }

        try {
            TargetResult r = handler_.handle(q);
            json out;
            out["lat_deg"]          = r.position.lat_deg;
            out["lon_deg"]          = r.position.lon_deg;
            out["alt_msl_m"]        = r.position.alt_m;
            out["ground_elev_m"]    = r.ground_elevation_m;
            out["agl_m"]            = r.target_height_agl_m;
            out["horiz_range_m"]    = r.horizontal_range_m;
            out["relative_elev_deg"]= r.relative_elevation_deg;
            res.set_content(out.dump(), "application/json");
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

    std::cout << "[server] Listening on port " << port_ << " -- Ctrl-C to stop\n";
    std::cout.flush();
    svr.listen("0.0.0.0", port_);
}
