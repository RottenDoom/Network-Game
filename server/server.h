#pragma once
#include "protocol.h"
#include "session.h"
#include <asio.hpp>
#include <memory>
#include <unordered_map>
#include <queue>

using asio::ip::tcp;

class Connection : public std::enable_shared_from_this<Connection>
{
public:
        Connection(tcp::socket socket, uint32_t id, class GameServer* server);

        void start();
        void send_message(const protocol::MessageBuffer& msg);
        uint32_t get_id() const { return player_id_; }

private:
        void read_header();
        void read_body(uint32_t length);
        void process_message(const std::vector<uint8_t>& data);
        void delayed_send(const protocol::MessageBuffer& msg);

        tcp::socket socket_;
        uint32_t player_id_;
        GameServer* server_;

        std::array<uint8_t, sizeof(protocol::MessageHeader)> header_buffer_;
        std::vector<uint8_t> body_buffer_;

        asio::steady_timer latency_timer_;
        std::queue<std::vector<uint8_t>> send_queue_;
};

class GameServer
{
public:
        GameServer(asio::io_context& io, uint16_t port);

        void start();
        void process_input(uint32_t player_id, const protocol::ClientInput& input);
        void player_disconnected(uint32_t player_id);

private:
        void accept_connection();
        void broadcast_state();

        asio::io_context& io_;
        tcp::acceptor acceptor_;
        asio::steady_timer broadcast_timer_;

        uint32_t next_player_id_;
        std::shared_ptr<GameSession> session_;
        std::unordered_map<uint32_t, std::shared_ptr<Connection>> connections_;
};