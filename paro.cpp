#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>

using json = nlohmann::json;
using boost::asio::ip::tcp;

class ABXClient {
private:
    boost::asio::io_context io_context;
    tcp::socket socket;
    std::map<int, json> packet_buffer;
    int expected_sequence{1};

public:
    ABXClient() : socket(io_context) {}

    bool connect(const std::string& host, const std::string& port) {
        try {
            tcp::resolver resolver(io_context);
            auto endpoints = resolver.resolve(host, port);
            boost::asio::connect(socket, endpoints);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return false;
        }
    }

    void receive_data() {
        std::vector<char> buffer(1024);
        
        while (true) {
            try {
                size_t length = socket.read_some(boost::asio::buffer(buffer));
                process_packet(buffer.data(), length);
            } catch (const std::exception& e) {
                std::cerr << "Read error: " << e.what() << std::endl;
                break;
            }
        }
    }

    void process_packet(const char* data, size_t length) {
        // Parse the received data into JSON
        // Note: Implement actual packet parsing based on ABX protocol
        json packet = json::parse(std::string(data, length));
        int sequence = packet["sequence"];
        
        packet_buffer[sequence] = packet;
        
        // Check for consecutive sequences
        while (packet_buffer.count(expected_sequence) > 0) {
            expected_sequence++;
        }
    }

    void save_to_file(const std::string& filename) {
        json output;
        json data_array = json::array();

        for (const auto& [seq, packet] : packet_buffer) {
            data_array.push_back(packet);
        }

        output["data"] = data_array;
        
        std::ofstream file(filename);
        file << output.dump(4);
        file.close();
    }
};

int main() {
    ABXClient client;
    
    if (client.connect("localhost", "8080")) {
        std::cout << "Connected to ABX server" << std::endl;
        client.receive_data();
        client.save_to_file("ticker_data.json");
    }

    return 0;
}