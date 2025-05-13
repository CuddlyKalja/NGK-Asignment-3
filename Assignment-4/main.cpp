#include <iostream>
#include <restinio/all.hpp>
#include <restinio/websocket/websocket.hpp>
#include <json_dto/pub.hpp>
#include <vector>
#include <algorithm>
#include <map>

namespace rws = restinio::websocket::basic;

struct weatherRegistration {
    weatherRegistration() = default;

    weatherRegistration(
        int ID,
        int Date,
        int Time,
        std::string placeName,
        double Lat,
        double Lon,
        double Temperature,
        int Humidity)
        : m_id{ ID }
        , m_date{ Date }
        , m_time{ Time }
        , m_placeName{ std::move(placeName) }
        , m_lat{ Lat }
        , m_lon{ Lon }
        , m_temperature{ Temperature }
        , m_humidity{ Humidity }
    {}

    template < typename JSON_IO >
    void json_io(JSON_IO &io) {
        io
            & json_dto::mandatory("ID", m_id)
            & json_dto::mandatory("Date", m_date)
            & json_dto::mandatory("Time", m_time)
            & json_dto::mandatory("PlaceName", m_placeName)
            & json_dto::mandatory("Lat", m_lat)
            & json_dto::mandatory("Lon", m_lon)
            & json_dto::mandatory("Temperature", m_temperature)
            & json_dto::mandatory("Humidity", m_humidity);
    }

    int m_id;
    int m_date;
    int m_time;
    std::string m_placeName;
    double m_lat;
    double m_lon;
    double m_temperature;
    int m_humidity;
};

using weatherStation_t = std::vector<weatherRegistration>;
namespace rr = restinio::router;
using router_t = rr::express_router_t<>;

class weatherInformationHandler {
public:
    explicit weatherInformationHandler(weatherStation_t &weather)
        : m_weather(weather) {}

    auto on_weather_list(const restinio::request_handle_t &req, rr::route_params_t) const {
        auto resp = init_resp(req->create_response());
        resp.set_body(json_dto::to_json(m_weather));
        return resp.done();
    }

    auto on_weather_post(const restinio::request_handle_t &req, rr::route_params_t) {
        try {
            auto body = req->body();
            auto newEntry = json_dto::from_json<weatherRegistration>(body);
            m_weather.push_back(std::move(newEntry));

            auto resp = init_resp(req->create_response(restinio::status_created()));
            resp.set_body(R"({"status": "added"})");

            // Push ny data til WebSocket-klienter
            auto json_msg = json_dto::to_json(m_weather.back());
            for (auto& [id, ws] : m_registry) {
                ws->send_message(json_msg);
            }

            return resp.done();
        }
        catch (const std::exception &ex) {
            return req->create_response(restinio::status_bad_request())
                .set_body(std::string("Error: ") + ex.what())
                .done();
        }
    }

    auto on_weather_put(const restinio::request_handle_t &req, rr::route_params_t params) {
        try {
            int id = std::stoi(std::string(params["id"]));
            auto updatedEntry = json_dto::from_json<weatherRegistration>(req->body());

            for (auto &entry : m_weather) {
                if (entry.m_id == id) {
                    entry = updatedEntry;
                    auto resp = init_resp(req->create_response());
                    resp.set_body(R"({"status": "updated"})");
                    return resp.done();
                }
            }

            return req->create_response(restinio::status_not_found())
                .set_body("ID not found")
                .done();
        }
        catch (...) {
            return req->create_response(restinio::status_bad_request())
                .set_body("Invalid data")
                .done();
        }
    }

    auto on_weather_by_id(const restinio::request_handle_t &req, rr::route_params_t params) const {
        int id = std::stoi(std::string(params["id"]));
        for (const auto &entry : m_weather) {
            if (entry.m_id == id) {
                auto resp = init_resp(req->create_response());
                resp.set_body(json_dto::to_json(entry));
                return resp.done();
            }
        }

        return req->create_response(restinio::status_not_found())
            .set_body("Not found")
            .done();
    }

    auto on_weather_by_date(const restinio::request_handle_t &req, rr::route_params_t params) const {
        int date = std::stoi(std::string(params["date"]));
        weatherStation_t results;
        for (const auto &entry : m_weather) {
            if (entry.m_date == date)
                results.push_back(entry);
        }

        auto resp = init_resp(req->create_response());
        resp.set_body(json_dto::to_json(results));
        return resp.done();
    }

    auto on_weather_latest(const restinio::request_handle_t &req, rr::route_params_t) const {
        weatherStation_t latest;
        int count = 0;
        for (auto it = m_weather.rbegin(); it != m_weather.rend() && count < 3; ++it, ++count)
            latest.push_back(*it);

        auto resp = init_resp(req->create_response());
        resp.set_body(json_dto::to_json(latest));
        return resp.done();
    }

    auto on_live_update(const restinio::request_handle_t& req, rr::route_params_t) {
        if (restinio::http_connection_header_t::upgrade == req->header().connection()) {
            auto wsh = rws::upgrade<traits_t>(*req, rws::activation_t::immediate,
                [this](auto wsh, auto m) {
                    if (rws::opcode_t::text_frame == m->opcode()) {
                        wsh->send_message(*m); // echo
                    }
                    else if (rws::opcode_t::connection_close_frame == m->opcode()) {
                        m_registry.erase(wsh->connection_id());
                    }
                });
            m_registry.emplace(wsh->connection_id(), wsh);
            return restinio::request_accepted();
        }
        return restinio::request_rejected();
    }

    auto on_options(const restinio::request_handle_t &req, rr::route_params_t) {
        return req->create_response()
            .append_header("Access-Control-Allow-Origin", "*")
            .append_header("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS")
            .append_header("Access-Control-Allow-Headers", "Content-Type")
            .done();
    }

private:
    weatherStation_t &m_weather;
    mutable std::map<std::uint64_t, rws::ws_handle_t> m_registry;

    template <typename RESP>
    static RESP init_resp(RESP resp) {
        resp
            .append_header("Server", "RESTinio WeatherServer")
            .append_header_date_field()
            .append_header("Content-Type", "application/json; charset=utf-8")
            .append_header("Access-Control-Allow-Origin", "*");
        return resp;
    }
};

auto server_handler(weatherStation_t &weatherStation) {
    auto router = std::make_unique<router_t>();
    auto handler = std::make_shared<weatherInformationHandler>(std::ref(weatherStation));

    router->http_get("/", std::bind(&weatherInformationHandler::on_weather_list, handler, std::placeholders::_1, std::placeholders::_2));
    router->http_post("/", std::bind(&weatherInformationHandler::on_weather_post, handler, std::placeholders::_1, std::placeholders::_2));
    router->http_put("/:id", std::bind(&weatherInformationHandler::on_weather_put, handler, std::placeholders::_1, std::placeholders::_2));
    router->http_get("/id/:id", std::bind(&weatherInformationHandler::on_weather_by_id, handler, std::placeholders::_1, std::placeholders::_2));
    router->http_get("/date/:date", std::bind(&weatherInformationHandler::on_weather_by_date, handler, std::placeholders::_1, std::placeholders::_2));
    router->http_get("/latest", std::bind(&weatherInformationHandler::on_weather_latest, handler, std::placeholders::_1, std::placeholders::_2));
    router->http_get("/chat", std::bind(&weatherInformationHandler::on_live_update, handler, std::placeholders::_1, std::placeholders::_2));

    router->add_handler(restinio::http_method_options(), "/", std::bind(&weatherInformationHandler::on_options, handler, std::placeholders::_1, std::placeholders::_2));
    router->add_handler(restinio::http_method_options(), R"(/:id(\d+))", std::bind(&weatherInformationHandler::on_options, handler, std::placeholders::_1, std::placeholders::_2));

    return router;
}

int main() {
    using namespace std::chrono;

    try {
        using traits_t = restinio::traits_t<
            restinio::asio_timer_manager_t,
            restinio::single_threaded_ostream_logger_t,
            router_t>;

        weatherStation_t weatherStation{
            {1, 20240415, 1015, "Aarhus N", 13.692, 19.438, 13.1, 70}
        };

        restinio::run(
            restinio::on_this_thread<traits_t>()
                .address("localhost")
                .port(8080)
                .request_handler(server_handler(weatherStation))
                .read_next_http_message_timelimit(10s)
                .write_http_response_timelimit(1s)
                .handle_request_timeout(1s));
    }
    catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
