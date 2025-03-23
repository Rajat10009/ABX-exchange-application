#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

constexpr int SERVER_PORT = 3000;
constexpr int PACKET_SIZE = 17;

struct StockPacket {
    std::string symbol;
    char buy_sell;
    int quantity;
    int price;
    int sequence;
};

std::vector<uint8_t> createRequestPacket(int callType, int sequence = 0) {
    std::vector<uint8_t> request(5, 0);
    request[0] = static_cast<uint8_t>(callType);
    for (int i = 1; i < 5; ++i) {
        request[i] = (sequence >> (8 * (4 - i))) & 0xFF;
    }
    return request;
}

StockPacket parsePacket(const std::vector<uint8_t>& data) {
    StockPacket packet;
    packet.symbol = std::string(data.begin(), data.begin() + 4);
    packet.buy_sell = data[4];
    packet.quantity = (data[5] << 24) | (data[6] << 16) | (data[7] << 8) | data[8];
    packet.price = (data[9] << 24) | (data[10] << 16) | (data[11] << 8) | data[12];
    packet.sequence = (data[13] << 24) | (data[14] << 16) | (data[15] << 8) | data[16];
    return packet;
}

void writeJSON(const std::map<int, StockPacket>& packets) {
    json output = json::array();
    for (const auto& [seq, packet] : packets) {
        output.push_back({
            {"symbol", packet.symbol},
            {"buy_sell", std::string(1, packet.buy_sell)},
            {"quantity", packet.quantity},
            {"price", packet.price},
            {"sequence", packet.sequence}
        });
    }
    std::ofstream file("output.json");
    file << output.dump(4);
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        socket.connect(tcp::endpoint(tcp::v4(), SERVER_PORT));
        
        std::map<int, StockPacket> receivedPackets;
        std::vector<uint8_t> request = createRequestPacket(1);
        boost::asio::write(socket, boost::asio::buffer(request));
        
        while (true) {
            std::vector<uint8_t> data(PACKET_SIZE);
            boost::asio::read(socket, boost::asio::buffer(data));
            
            StockPacket packet = parsePacket(data);
            receivedPackets[packet.sequence] = packet;
            
            if (receivedPackets.size() > 1) {
                auto prev = receivedPackets.rbegin();
                auto current = prev++;
                if (prev->first + 1 != current->first) {
                    std::vector<uint8_t> req = createRequestPacket(2, prev->first + 1);
                    boost::asio::write(socket, boost::asio::buffer(req));
                }
            }
        }
        
        writeJSON(receivedPackets);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
