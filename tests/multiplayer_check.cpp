// Runs one multiplayer cartridge on several PRG32 runtimes at once, each with its own
// prg32-multiplayer-1.0 connection, against a real PRG32 Multiplayer Server:
//
//     prg32qt_multiplayer_check cartridge.prg32 ws://localhost:18081/ [consoles] [seconds]
//
// Every runtime calls the multiplayer ABI (#32-#40, #139) from the cartridge itself, as the
// desktop app does. Passes when every console was admitted with its own server-assigned player ID
// (ABI #139), sees every other console as a peer (ABI #39/#40) with that console's ID, and drew a
// frame. Without a server address (argument or PRG32QT_MULTIPLAYER_URL) it exits 77, a CTest skip.
#include "Cartridge.h"
#include "QtMultiplayerService.h"
#include "Runtime.h"
#include <QCoreApplication>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

namespace {
/** The firmware's frame period: 30 frames per second. */
constexpr int FramePeriodMs = 33;
/** Two consoles are allowed this long to connect, be welcomed and exchange snapshots. */
constexpr int DefaultSeconds = 5;
constexpr int DefaultConsoles = 2;
constexpr int MaximumConsoles = 8;

struct Console {
    std::unique_ptr<QtMultiplayerService> service;
    std::unique_ptr<prg32::Runtime> runtime;
};

uint32_t nonBlackPixels(const prg32::Runtime& runtime) {
    const std::vector<uint16_t> pixels = runtime.framebuffer().rgb565Pixels();
    return uint32_t(std::count_if(pixels.begin(), pixels.end(), [](uint16_t pixel) { return pixel != 0; }));
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    // A settings scope of its own, so the check never touches the desktop app's saved server address.
    QCoreApplication::setOrganizationName(QStringLiteral("PRG32-QT-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("multiplayer-check"));
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: prg32qt_multiplayer_check cartridge.prg32 [ws-url] [consoles] [seconds]\n");
        return 2;
    }
    QString url =
        argc > 2 ? QString::fromLocal8Bit(argv[2]) : qEnvironmentVariable("PRG32QT_MULTIPLAYER_URL");
    if (url.isEmpty() || url == QLatin1String("-")) {
        std::printf("SKIP: set PRG32QT_MULTIPLAYER_URL to a PRG32 Multiplayer Server (ws://host:port/)\n");
        return 77;
    }
    const int consoles = std::clamp(argc > 3 ? std::atoi(argv[3]) : DefaultConsoles, 2, MaximumConsoles);
    const int seconds = std::max(1, argc > 4 ? std::atoi(argv[4]) : DefaultSeconds);

    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    std::string error;
    auto cartridge = prg32::Cartridge::parse(bytes, error);
    if (!cartridge) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }

    std::vector<Console> fleet(static_cast<size_t>(consoles));
    for (Console& console : fleet) {
        console.service = std::make_unique<QtMultiplayerService>();
        console.service->setServerUrl(url);
        console.runtime = std::make_unique<prg32::Runtime>();
        console.runtime->setMultiplayerService(console.service.get());
        if (!console.runtime->load(*cartridge, error) || !console.runtime->init(error)) {
            std::fprintf(stderr, "FAIL: %s\n", error.c_str());
            return 1;
        }
    }

    bool failed = false;
    QTimer frames;
    QObject::connect(&frames, &QTimer::timeout, [&] {
        for (Console& console : fleet) {
            if (!console.runtime->frame(0, error)) {
                std::fprintf(stderr, "FAIL: frame: %s\n", error.c_str());
                failed = true;
                app.exit(1);
                return;
            }
        }
    });
    frames.start(FramePeriodMs);
    QTimer::singleShot(seconds * 1000, &app, [&] { app.exit(0); });
    if (app.exec() != 0 || failed)
        return 1;
    frames.stop();

    std::set<uint32_t> ids;
    for (const Console& console : fleet)
        ids.insert(console.service->localPlayerId());
    bool pass = !ids.count(0) && ids.size() == fleet.size();
    for (size_t index = 0; index < fleet.size(); ++index) {
        prg32::MultiplayerService& service = *fleet[index].service;
        const uint32_t self = service.localPlayerId();
        std::set<uint32_t> peers;
        const int count = service.peerCount(0);
        for (int peerIndex = 0; peerIndex < count; ++peerIndex) {
            prg32::MultiplayerPeer peer;
            if (service.peer(peerIndex, 0, peer) == 0)
                peers.insert(peer.playerId);
        }
        std::set<uint32_t> expected = ids;
        expected.erase(self);
        // Consoles of other hosts (PRG32-iOS, QEMU, a board) may share the room: count them, require ours.
        const size_t outsiders =
            std::count_if(peers.begin(), peers.end(), [&](uint32_t id) { return !ids.count(id); });
        const uint32_t drawn = nonBlackPixels(*fleet[index].runtime);
        std::printf("CONSOLE %zu player_id=%u peers=%d other_hosts=%zu non_black=%u\n",
                    index,
                    self,
                    count,
                    outsiders,
                    drawn);
        pass =
            pass && std::includes(peers.begin(), peers.end(), expected.begin(), expected.end()) && drawn > 0;
    }
    std::printf("%s: %d consoles on %s\n", pass ? "PASS" : "FAIL", consoles, qPrintable(url));
    return pass ? 0 : 1;
}
