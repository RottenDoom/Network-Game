/**
 * @file protocol.h
 * @brief Network protocol definitions for client-server communication
 * @author NetworkGame Project
 * @date 2024
 */

#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

/**
 * @namespace protocol
 * @brief Network protocol definitions and message serialization
 */
namespace protocol
{

        /**
         * @enum MessageType
         * @brief Types of messages exchanged between client and server
         */
        enum class MessageType : uint8_t
        {
                CLIENT_CONNECT    = 1,
                CLIENT_INPUT      = 2,
                SERVER_GAME_STATE = 3,
                SERVER_START_GAME = 4,
                CLIENT_DISCONNECT = 5  ///< Client disconnection notification
        };

        /**
         * @struct Vec2
         * @brief 2D vector representing position or direction
         */
        struct Vec2
        {
                float x, y;
                Vec2() : x(0), y(0) {}
                /**
                 * @brief Construct Vec2 with coordinates
                 * @param x X coordinate
                 * @param y Y coordinate
                 */
                Vec2(float x, float y) : x(x), y(y) {}
        };

        /**
         * @struct PlayerState
         * @brief Represents a player's current game state
         */
        struct PlayerState
        {
                uint32_t id;
                Vec2 position;
                uint32_t score;
                uint32_t last_processed_input_seq;  // appended by server
                uint32_t last_processed_input_ts;   ///< Server-recorded client input timestamp (ms)
        };

        /**
         * @struct CoinState
         * @brief Represents a coin's position in the game world
         */
        struct CoinState
        {
                uint32_t id;
                Vec2 position;  ///< Coin position in world coordinates
        };

        /**
         * @struct ClientInput
         * @brief Input data sent from client to server
         */
        struct ClientInput
        {
                float dx, dy;        ///< Movement direction vector (normalized)
                uint32_t timestamp;  ///< Client timestamp when input was generated (ms)
                uint32_t seq;        ///< Input sequence number for reconciliation
        };

        /**
         * @struct GameStateMessage
         * @brief Complete game state broadcast by server
         * @note Followed by PlayerState[player_count] and CoinState[coin_count]
         */
        struct GameStateMessage
        {
                uint32_t timestamp;    ///< Server timestamp (ms)
                uint8_t player_count;  ///< Number of players in the game
                uint8_t coin_count;    ///< Number of active coins
        };

        /**
         * @struct MessageHeader
         * @brief Header prepended to all network messages
         */
        struct MessageHeader
        {
                MessageType type;
                uint32_t length;  ///< Total message length including header
        };

        /**
         * @class MessageBuffer
         * @brief Serializes game data into network messages
         */
        class MessageBuffer
        {
        public:
                std::vector<uint8_t> data;  ///< Raw message buffer

                /**
                 * @brief Write message header
                 * @param type Message type identifier
                 */
                void write_header(MessageType type)
                {
                        MessageHeader header{type, 0};
                        size_t start = data.size();
                        data.resize(start + sizeof(MessageHeader));
                        std::memcpy(data.data() + start, &header, sizeof(MessageHeader));
                }

                /**
                 * @brief Write 32-bit unsigned integer
                 * @param value Value to write
                 */
                void write_uint32(uint32_t value)
                {
                        size_t start = data.size();
                        data.resize(start + sizeof(uint32_t));
                        std::memcpy(data.data() + start, &value, sizeof(uint32_t));
                }

                /**
                 * @brief Write 32-bit float
                 * @param value Value to write
                 */
                void write_float(float value)
                {
                        size_t start = data.size();
                        data.resize(start + sizeof(float));
                        std::memcpy(data.data() + start, &value, sizeof(float));
                }

                /**
                 * @brief Write 2D vector
                 * @param v Vector to write
                 */
                void write_vec2(const Vec2& v)
                {
                        write_float(v.x);
                        write_float(v.y);
                }

                /**
                 * @brief Write player state
                 * @param ps Player state to serialize
                 */
                void write_player_state(const PlayerState& ps)
                {
                        write_uint32(ps.id);
                        write_vec2(ps.position);
                        write_uint32(ps.score);
                        write_uint32(ps.last_processed_input_seq);
                        write_uint32(ps.last_processed_input_ts);
                }

                /**
                 * @brief Write coin state
                 * @param cs Coin state to serialize
                 */
                void write_coin_state(const CoinState& cs)
                {
                        write_uint32(cs.id);
                        write_vec2(cs.position);
                }

                /**
                 * @brief Finalize message by updating header length
                 * @note Must be called after all data is written
                 */
                void finalize()
                {
                        if (data.size() >= sizeof(MessageHeader))
                        {
                                MessageHeader* header = reinterpret_cast<MessageHeader*>(data.data());
                                header->length        = static_cast<uint32_t>(data.size());
                        }
                }
        };

        /**
         * @class MessageReader
         * @brief Deserializes network messages into game data
         */
        class MessageReader
        {
        public:
                const uint8_t* data;  ///< Pointer to message data
                size_t size;          ///< Total message size
                size_t offset;        ///< Current read offset

                /**
                 * @brief Construct message reader
                 * @param d Pointer to message data
                 * @param s Message size in bytes
                 */
                MessageReader(const uint8_t* d, size_t s) : data(d), size(s), offset(0) {}

                /**
                 * @brief Read message header
                 * @param header Output header structure
                 * @return true if successful, false on error
                 */
                bool read_header(MessageHeader& header)
                {
                        if (offset + sizeof(MessageHeader) > size)
                                return false;
                        std::memcpy(&header, data + offset, sizeof(MessageHeader));
                        offset += sizeof(MessageHeader);
                        return true;
                }

                /**
                 * @brief Read 32-bit unsigned integer
                 * @param value Output value
                 * @return true if successful, false on error
                 */
                bool read_uint32(uint32_t& value)
                {
                        if (offset + sizeof(uint32_t) > size)
                                return false;
                        std::memcpy(&value, data + offset, sizeof(uint32_t));
                        offset += sizeof(uint32_t);
                        return true;
                }

                /**
                 * @brief Read 32-bit float
                 * @param value Output value
                 * @return true if successful, false on error
                 */
                bool read_float(float& value)
                {
                        if (offset + sizeof(float) > size)
                                return false;
                        std::memcpy(&value, data + offset, sizeof(float));
                        offset += sizeof(float);
                        return true;
                }

                /**
                 * @brief Read 2D vector
                 * @param v Output vector
                 * @return true if successful, false on error
                 */
                bool read_vec2(Vec2& v) { return read_float(v.x) && read_float(v.y); }

                /**
                 * @brief Read player state
                 * @param ps Output player state
                 * @return true if successful, false on error
                 */
                bool read_player_state(PlayerState& ps)
                {
                        return read_uint32(ps.id) && read_vec2(ps.position) && read_uint32(ps.score) &&
                               read_uint32(ps.last_processed_input_seq) && read_uint32(ps.last_processed_input_ts);
                }

                /**
                 * @brief Read coin state
                 * @param cs Output coin state
                 * @return true if successful, false on error
                 */
                bool read_coin_state(CoinState& cs) { return read_uint32(cs.id) && read_vec2(cs.position); }
        };

}  // namespace protocol