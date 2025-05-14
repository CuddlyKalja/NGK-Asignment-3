#include <iostream>
#include <restinio/all.hpp>
#include <json_dto/pub.hpp>

// --- Weather registration structure ---
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

// --- Handler class ---
class weatherInformationHandler {
    public:
        explicit weatherInformationHandler(weatherStation_t &weather)
            : m_weather(weather) {}
    
        auto on_weather_list(const restinio::request_handle_t &req, rr::route_params_t) const {
            auto resp = init_resp(req->create_response());
            resp.set_body(json_dto::to_json(m_weather));
            return resp.done();
        }
    
    private:
        weatherStation_t &m_weather;
    
        template <typename RESP>
        static RESP init_resp(RESP resp) {
            resp
                .append_header("Server", "RESTinio WeatherServer")
                .append_header_date_field()
                .append_header("Content-Type", "application/json; charset=utf-8");
    
            return resp;
        }
    };
    
    // --- Server setup ---
    auto server_handler(weatherStation_t &weatherStation) {
        auto router = std::make_unique<router_t>();
        auto handler = std::make_shared<weatherInformationHandler>(std::ref(weatherStation));
    
        router->http_get("/", std::bind(&weatherInformationHandler::on_weather_list, handler, std::placeholders::_1, std::placeholders::_2));
    
        router->add_handler(
            rr::none_of_methods(restinio::http_method_get()),
            "/",
            [](const auto &req, auto) {
                return req->create_response(restinio::status_method_not_allowed())
                    .connection_close()
                    .done();
            });
    
        return router;
    }
    // --- Main ---
    int main() {
        using namespace std::chrono;
    
        try {
            using traits_t = restinio::traits_t<
                restinio::asio_timer_manager_t,
                restinio::single_threaded_ostream_logger_t,
                router_t>;
    
            // Hardcoded weather data (per the assignment)
            weatherStation_t weatherStation{
                {1, 20240415, 1015, "Aarhus N", 13.692, 19.438, 10.1, 70}
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