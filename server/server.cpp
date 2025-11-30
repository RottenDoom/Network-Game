#include "server.h"
#include <iostream>

Connection::Connection(tcp::socket socket, uint32_t id, GameServer* server)
    : socket_(std::move(socket)), player_id_(id), server_(server), latency_timer_(socket_.get_executor())
{
}

void Connection::start()
{
        std::cout << "Connection " << player_id_ << " started\n";
        read_header();
}

void Connection::read_header()
{
        auto self = shared_from_this();
        asio::async_read(socket_,
                         asio::buffer(header_buffer_),
                         [self](asio::error_code ec, std::size_t)
                         {
                                 if (!ec)
                                 {
                                         protocol::MessageHeader header;
                                         std::memcpy(&header, self->header_buffer_.data(), sizeof(header));

                                         if (header.length > sizeof(protocol::MessageHeader) && header.length < 65536)
                                         {
                                                 self->read_body(header.length - sizeof(protocol::MessageHeader));
                                         }
                                         else
                                         {
                                                 self->read_header();
                                         }
                                 }
                                 else
                                 {
                                         std::cout << "Connection " << self->player_id_ << " error: " << ec.message()
                                                   << "\n";
                                         self->server_->player_disconnected(self->player_id_);
                                 }
                         });
}

void Connection::read_body(uint32_t length)
{
        body_buffer_.resize(length);
        auto self = shared_from_this();

        asio::async_read(
            socket_,
            asio::buffer(body_buffer_),
            [self](asio::error_code ec, std::size_t)
            {
                    if (!ec)
                    {
                            std::vector<uint8_t> full_msg;
                            full_msg.insert(full_msg.end(), self->header_buffer_.begin(), self->header_buffer_.end());
                            full_msg.insert(full_msg.end(), self->body_buffer_.begin(), self->body_buffer_.end());

                            // Simulate 200ms receive latency
                            self->latency_timer_.expires_after(std::chrono::milliseconds(200));
                            self->latency_timer_.async_wait(
                                [self, msg = std::move(full_msg)](auto ec)
                                {
                                        if (!ec)
                                                self->process_message(msg);
                                });

                            self->read_header();
                    }
                    else
                    {
                            std::cout << "Connection " << self->player_id_ << " body error\n";
                            self->server_->player_disconnected(self->player_id_);
                    }
            });
}

void Connection::process_message(const std::vector<uint8_t>& data)
{
        protocol::MessageReader reader(data.data(), data.size());
        protocol::MessageHeader header;

        if (!reader.read_header(header))
                return;

        switch (header.type)
        {
        case protocol::MessageType::CLIENT_CONNECT:
                std::cout << "Player " << player_id_ << " connected\n";
                break;

        case protocol::MessageType::CLIENT_INPUT:
        {
                protocol::ClientInput input;
                if (reader.read_float(input.dx) && reader.read_float(input.dy) && reader.read_uint32(input.timestamp))
                {
                        server_->process_input(player_id_, input);
                }
                break;
        }

        default:
                break;
        }
}

void Connection::send_message(const protocol::MessageBuffer& msg)
{
        delayed_send(msg);
}

void Connection::delayed_send(const protocol::MessageBuffer& msg)
{
        auto self = shared_from_this();

        // Simulate 200ms send latency
        latency_timer_.expires_after(std::chrono::milliseconds(200));
        latency_timer_.async_wait(
            [self, data = msg.data](auto ec)
            {
                    if (!ec)
                    {
                            asio::async_write(self->socket_,
                                              asio::buffer(data),
                                              [self](asio::error_code ec, std::size_t)
                                              {
                                                      if (ec)
                                                      {
                                                              std::cout << "Send error to " << self->player_id_ << "\n";
                                                      }
                                              });
                    }
            });
}

GameServer::GameServer(asio::io_context& io, uint16_t port)
    : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)), broadcast_timer_(io), next_player_id_(1)
{
        session_ = std::make_shared<GameSession>(io);
}

void GameServer::start()
{
        std::cout << "Server started on port " << acceptor_.local_endpoint().port() << "\n";
        accept_connection();
        broadcast_state();
}

void GameServer::accept_connection()
{
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket)
            {
                    if (!ec)
                    {
                            uint32_t id      = next_player_id_++;
                            auto conn        = std::make_shared<Connection>(std::move(socket), id, this);
                            connections_[id] = conn;
                            session_->add_player(id);
                            conn->start();

                            // Start game when 2 players connect
                            if (connections_.size() >= 2 && connections_.size() == 2)
                            {
                                    session_->start();
                            }
                    }
                    accept_connection();
            });
}

void GameServer::broadcast_state()
{
        auto state_msg = session_->create_state_message();

        for (auto& [id, conn] : connections_)
        {
                conn->send_message(state_msg);
        }

        broadcast_timer_.expires_after(std::chrono::milliseconds(50));
        broadcast_timer_.async_wait(
            [this](auto ec)
            {
                    if (!ec)
                            broadcast_state();
            });
}

void GameServer::process_input(uint32_t player_id, const protocol::ClientInput& input)
{
        session_->process_input(player_id, input);
}

void GameServer::player_disconnected(uint32_t player_id)
{
        connections_.erase(player_id);
        session_->remove_player(player_id);
}