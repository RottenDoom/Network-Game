/**
 * @file main.cpp
 * @brief Client application entry point
 * @author NetworkGame Project
 * @date 2024
 */

#include "client.h"
#include "renderer.h"
#include <iostream>
#include <thread>

/**
 * @brief Main client application
 * @param argc Argument count
 * @param argv Arguments: [host] [port]
 * @return 0 on success, 1 on error
 */
int main(int argc, char* argv[])
{
        try
        {
                std::string host = "127.0.0.1";
                uint16_t port    = 12345;

                if (argc > 1)
                        host = argv[1];
                if (argc > 2)
                        port = static_cast<uint16_t>(std::atoi(argv[2]));

                asio::io_context io;
                GameClient client(io);

                // Run networking in separate thread
                std::thread network_thread(
                    [&io, &client, host, port]()
                    {
                            try
                            {
                                    client.connect(host, port);
                                    io.run();
                            }
                            catch (std::exception& e)
                            {
                                    std::cerr << "Network error: " << e.what() << "\n";
                            }
                    });

                // Rendering on main thread
                Renderer renderer(800, 600);
                if (!renderer.init())
                {
                        io.stop();
                        network_thread.join();
                        return 1;
                }

                SDL_Event event;
                auto last_frame = std::chrono::steady_clock::now();

                bool keys[4]    = {false};  // W, A, S, D

                while (renderer.is_running())
                {
                        // Handle events
                        while (SDL_PollEvent(&event))
                        {
                                if (event.type == SDL_QUIT)
                                {
                                        renderer.set_running(false);
                                }
                                else if (event.type == SDL_KEYDOWN)
                                {
                                        switch (event.key.keysym.sym)
                                        {
                                        case SDLK_w:
                                        case SDLK_UP:
                                                keys[0] = true;
                                                break;
                                        case SDLK_a:
                                        case SDLK_LEFT:
                                                keys[1] = true;
                                                break;
                                        case SDLK_s:
                                        case SDLK_DOWN:
                                                keys[2] = true;
                                                break;
                                        case SDLK_d:
                                        case SDLK_RIGHT:
                                                keys[3] = true;
                                                break;
                                        case SDLK_ESCAPE:
                                                renderer.set_running(false);
                                                break;
                                        }
                                }
                                else if (event.type == SDL_KEYUP)
                                {
                                        switch (event.key.keysym.sym)
                                        {
                                        case SDLK_w:
                                        case SDLK_UP:
                                                keys[0] = false;
                                                break;
                                        case SDLK_a:
                                        case SDLK_LEFT:
                                                keys[1] = false;
                                                break;
                                        case SDLK_s:
                                        case SDLK_DOWN:
                                                keys[2] = false;
                                                break;
                                        case SDLK_d:
                                        case SDLK_RIGHT:
                                                keys[3] = false;
                                                break;
                                        }
                                }
                        }

                        // Calculate delta time
                        auto now   = std::chrono::steady_clock::now();
                        float dt   = std::chrono::duration<float>(now - last_frame).count();
                        last_frame = now;

                        // Send input
                        float dx = 0.0f, dy = 0.0f;
                        if (keys[0])
                                dy -= 1.0f;  // W
                        if (keys[1])
                                dx -= 1.0f;  // A
                        if (keys[2])
                                dy += 1.0f;  // S
                        if (keys[3])
                                dx += 1.0f;  // D

                        if (dx != 0.0f || dy != 0.0f)
                        {
                                // Apply local prediction immediately so movement
                                // feels responsive while waiting for the server.
                                client.apply_local_input(dx, dy, dt);
                                client.send_input(dx, dy);
                        }

                        // Update interpolation
                        client.update_interpolation(dt);

                        // Render
                        renderer.render(client);

                        // Cap at 60 FPS
                        SDL_Delay(16);
                }

                io.stop();
                network_thread.join();
        }
        catch (std::exception& e)
        {
                std::cerr << "Client error: " << e.what() << "\n";
                return 1;
        }

        return 0;
}