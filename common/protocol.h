#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace protocol
{

        enum class MessageType : uint8_t
        {
                CLIENT_CONNECT    = 1,
                CLIENT_INPUT      = 2,
                SERVER_GAME_STATE = 3,
                SERVER_START_GAME = 4,
                CLIENT_DISCONNECT = 5
        };

        struct Vec2
        {
                float x, y;
                Vec2() : x(0), y(0) {}
                Vec2(float x, float y) : x(x), y(y) {}
        };

        struct PlayerState
        {
                uint32_t id;
                Vec2 position;
                uint32_t score;
                uint32_t last_processed_input_seq;  // appended by server
        };

        struct CoinState
        {
                uint32_t id;
                Vec2 position;
        };

        struct ClientInput
        {
                float dx, dy;  // Movement direction
                uint32_t timestamp;
                uint32_t seq;  // input sequence number for reconciliation
        };

        struct GameStateMessage
        {
                uint32_t timestamp;
                uint8_t player_count;
                uint8_t coin_count;
                // Followed by PlayerState[player_count] and CoinState[coin_count]
        };

        // Message header
        struct MessageHeader
        {
                MessageType type;
                uint32_t length;
        };

        class MessageBuffer
        {
        public:
                std::vector<uint8_t> data;

                void write_header(MessageType type)
                {
                        MessageHeader header{type, 0};
                        size_t start = data.size();
                        data.resize(start + sizeof(MessageHeader));
                        std::memcpy(data.data() + start, &header, sizeof(MessageHeader));
                }

                void write_uint32(uint32_t value)
                {
                        size_t start = data.size();
                        data.resize(start + sizeof(uint32_t));
                        std::memcpy(data.data() + start, &value, sizeof(uint32_t));
                }

                void write_float(float value)
                {
                        size_t start = data.size();
                        data.resize(start + sizeof(float));
                        std::memcpy(data.data() + start, &value, sizeof(float));
                }

                void write_vec2(const Vec2& v)
                {
                        write_float(v.x);
                        write_float(v.y);
                }

                void write_player_state(const PlayerState& ps)
                {
                        write_uint32(ps.id);
                        write_vec2(ps.position);
                        write_uint32(ps.score);
                        write_uint32(ps.last_processed_input_seq);
                }

                void write_coin_state(const CoinState& cs)
                {
                        write_uint32(cs.id);
                        write_vec2(cs.position);
                }

                void finalize()
                {
                        if (data.size() >= sizeof(MessageHeader))
                        {
                                MessageHeader* header = reinterpret_cast<MessageHeader*>(data.data());
                                header->length        = static_cast<uint32_t>(data.size());
                        }
                }
        };

        class MessageReader
        {
        public:
                const uint8_t* data;
                size_t size;
                size_t offset;

                MessageReader(const uint8_t* d, size_t s) : data(d), size(s), offset(0) {}

                bool read_header(MessageHeader& header)
                {
                        if (offset + sizeof(MessageHeader) > size)
                                return false;
                        std::memcpy(&header, data + offset, sizeof(MessageHeader));
                        offset += sizeof(MessageHeader);
                        return true;
                }

                bool read_uint32(uint32_t& value)
                {
                        if (offset + sizeof(uint32_t) > size)
                                return false;
                        std::memcpy(&value, data + offset, sizeof(uint32_t));
                        offset += sizeof(uint32_t);
                        return true;
                }

                bool read_float(float& value)
                {
                        if (offset + sizeof(float) > size)
                                return false;
                        std::memcpy(&value, data + offset, sizeof(float));
                        offset += sizeof(float);
                        return true;
                }

                bool read_vec2(Vec2& v) { return read_float(v.x) && read_float(v.y); }

                bool read_player_state(PlayerState& ps)
                {
                        return read_uint32(ps.id) && read_vec2(ps.position) && read_uint32(ps.score) &&
                               read_uint32(ps.last_processed_input_seq);
                }

                bool read_coin_state(CoinState& cs) { return read_uint32(cs.id) && read_vec2(cs.position); }
        };

}  // namespace protocol