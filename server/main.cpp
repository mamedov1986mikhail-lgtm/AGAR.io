#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "server.hpp"

namespace {

void usage(const char* exe) {
    std::printf(
        "Использование: %s [--host ADDR] [--tcp PORT] [--udp PORT] [--bots N] [--records FILE]\n"
        "  --host     адрес прослушивания (по умолчанию 0.0.0.0)\n"
        "  --tcp      TCP-порт (по умолчанию %d)\n"
        "  --udp      UDP-порт (по умолчанию %d)\n"
        "  --bots N   держать до N ботов на свободных местах (по умолчанию 0)\n"
        "  --records  файл таблицы рекордов (по умолчанию records.txt)\n",
        exe, agar::kDefaultTcpPort, agar::kDefaultUdpPort);
}

uv_signal_t g_sigint, g_sigterm;

void onSignal(uv_signal_t* h, int) {
    std::printf("\nОстановка сервера...\n");
    static_cast<agar::Server*>(h->data)->stop();
    // Закрываем оба обработчика, иначе цикл событий не завершится.
    for (uv_signal_t* s : {&g_sigint, &g_sigterm})
        if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(s)))
            uv_close(reinterpret_cast<uv_handle_t*>(s), nullptr);
}

}  // namespace

int main(int argc, char** argv) {
    agar::ServerConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        const char* v = nullptr;
        if (a == "--help" || a == "-h") {
            usage(argv[0]);
            return 0;
        } else if (a == "--host" && (v = next())) {
            cfg.host = v;
        } else if (a == "--tcp" && (v = next())) {
            cfg.tcpPort = std::atoi(v);
        } else if (a == "--udp" && (v = next())) {
            cfg.udpPort = std::atoi(v);
        } else if (a == "--bots" && (v = next())) {
            cfg.bots = std::atoi(v);
        } else if (a == "--records" && (v = next())) {
            cfg.recordsFile = v;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    uv_loop_t* loop = uv_default_loop();
    agar::Server server(loop, cfg);
    if (!server.start()) return 1;

    uv_signal_init(loop, &g_sigint);
    uv_signal_init(loop, &g_sigterm);
    g_sigint.data = &server;
    g_sigterm.data = &server;
    uv_signal_start(&g_sigint, onSignal, SIGINT);
    uv_signal_start(&g_sigterm, onSignal, SIGTERM);

    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    return 0;
}
