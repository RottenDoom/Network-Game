/**
 * @file server.h
 * @brief Game server networking and connection management
 * @author NetworkGame Project
 * @date 2024
 */

#pragma once
#include "protocol.h"
#include "session.h"
#include <asio.hpp>
#include <memory>
#include <unordered_map>
#include <queue>

using asio::ip::tcp;

/**
 * @class Connection
 * @brief Manages a single client connection with message handling and latency simulation
 */
class Connection : public std::enable_shared_from_this<Connection>
{
public:
        /**
         * @brief Construct connection
         * @param socket TCP socket for this connection
         * @param id Unique player ID assigned to this connection
         * @param server Pointer to game server
         */
        Connection(tcp::socket socket, uint32_t id, class GameServer* server);

        /**
         * @brief Start reading messages from client
         */
        void start();

        /**
         * @brief Send message to client (with simulated latency)
         * @param msg Message buffer to send
         */
        void send_message(const protocol::MessageBuffer& msg);

        /**
         * @brief Get player ID for this connection
         * @return Player ID
         */
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
        std::queue<std::vector<uint8_t>> send_queue_;  ///< Queue of pending messages
};

/**
 * @class GameServer
 * @brief Main server class managing connections and game session
 */
class GameServer
{
public:
        /**
         * @brief Construct game server
         * @param io ASIO I/O context
         * @param port Port to listen on
         */
        GameServer(asio::io_context& io, uint16_t port);

        /**
         * @brief Start accepting connections and broadcasting game state
         */
        void start();

        /**
         * @brief Process input from a client
         * @param player_id Player ID
         * @param input Client input data
         */
        void process_input(uint32_t player_id, const protocol::ClientInput& input);

        /**
         * @brief Handle player disconnection
         * @param player_id Player ID that disconnected
         */
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