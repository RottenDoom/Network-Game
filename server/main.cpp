#include "server.h"
#include <iostream>

int main(int argc, char* argv[])
{
        try
        {
                uint16_t port = 12345;
                if (argc > 1)
                {
                        port = static_cast<uint16_t>(std::atoi(argv[1]));
                }

                asio::io_context io;
                GameServer server(io, port);
                server.start();

                std::cout << "Server running. Press Ctrl+C to stop.\n";
                io.run();
        }
        catch (std::exception& e)
        {
                std::cerr << "Server error: " << e.what() << "\n";
                return 1;
        }

        return 0;
}