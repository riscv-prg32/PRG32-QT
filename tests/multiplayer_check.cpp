// Runs one multiplayer cartridge on several PRG32 runtimes at once, each with its own
// prg32-multiplayer-1.0 connection, against a real PRG32 Multiplayer Server:
//
//     prg32qt_multiplayer_check cartridge.prg32 ws://localhost:18081/ [consoles] [seconds]
//     prg32qt_multiplayer_check cartridge.prg32 discover [consoles] [seconds]
//     prg32qt_multiplayer_check --wire
//
// Options after the positional arguments:
//     --keys A@60,DOWN@90,LEFT+A@120   taps on every console: each key, or keys joined by '+', is held
//                                      for 4 frames from the given frame, so that the cartridge sees a
//                                      complete press and release (names: LEFT RIGHT UP DOWN A B SELECT
//                                      C D L R)
//     --dump DIRECTORY                 write each console's last frame as console-N.ppm
// The keys walk a Store cartridge's menus to its online lobby; most join only there.
//
// `discover` sets no address: each console finds a server by LAN discovery (UDP 8082), as the firmware
// does, and the check also requires that every console marks it as local. `--wire` checks the address
// and discovery helpers against the firmware's tests/multiplayer_wire_test.c cases, with no network.
//
// Every runtime calls the multiplayer ABI (#32-#40, #139) from the cartridge itself, as the
// desktop app does. Passes when every console was admitted with its own server-assigned player ID
// (ABI #139), sees every other console as a peer (ABI #39/#40) with that console's ID, and drew a
// frame. Without a server address (argument or PRG32QT_MULTIPLAYER_URL) it exits 77, a CTest skip.
#include "Cartridge.h"
#include "QtMultiplayerService.h"
#include "Runtime.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMap>
#include <QStringList>
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

/** Count a failed expectation of the --wire checks. */
int expect(bool condition, const char* what) {
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", what);
    return condition ? 0 : 1;
}

/** The firmware's prg32_mp_normalize_server_url(), prg32_mp_valid_server_url() and
 * prg32_mp_parse_discovery() cases, against QtMultiplayerService's helpers. */
int wireChecks() {
    using Service = QtMultiplayerService;
    int failures = 0;
    failures += expect(Service::normalizeServerUrl("192.168.1.10") == "ws://192.168.1.10:8081", "bare host");
    failures += expect(Service::normalizeServerUrl("host:9000") == "ws://host:9000", "host:port");
    failures += expect(Service::normalizeServerUrl("wss://multi.prg32.uniparthenope.it") ==
                           "wss://multi.prg32.uniparthenope.it",
                       "wss URL kept");
    failures += expect(Service::normalizeServerUrl("http://host").isEmpty(), "http refused");
    failures += expect(Service::normalizeServerUrl("host:0").isEmpty(), "port 0 refused");
    failures += expect(Service::normalizeServerUrl("host:65536").isEmpty(), "port 65536 refused");
    failures += expect(Service::normalizeServerUrl("host:80a").isEmpty(), "non-digit port refused");
    failures += expect(Service::normalizeServerUrl("bad host").isEmpty(), "space refused");
    failures += expect(Service::normalizeServerUrl("").isEmpty(), "empty is no address");
    failures += expect(!Service::validServerUrl("ws://"), "no host");
    failures += expect(!Service::validServerUrl("ws://:8081"), "port without host");
    failures += expect(!Service::validServerUrl("ws:///path"), "path without host");
    failures += expect(!Service::validServerUrl("ws://" + QString(123, 'a')), "128 characters refused");
    failures += expect(Service::validServerUrl("ws://" + QString(122, 'a')), "127 characters accepted");
    const QByteArray good =
        R"({"type":"server","protocol":"prg32-multiplayer-1.0","url":"ws://10.0.0.2:8081"})";
    failures += expect(Service::parseDiscovery(good) == "ws://10.0.0.2:8081", "server answer");
    failures +=
        expect(Service::parseDiscovery(R"({"type":"discover","protocol":"prg32-multiplayer-1.0"})").isEmpty(),
               "a query is not an answer");
    failures += expect(
        Service::parseDiscovery(R"({"type":"server","protocol":"other-1.0","url":"ws://10.0.0.2:8081"})")
            .isEmpty(),
        "foreign protocol");
    failures += expect(Service::parseDiscovery(
                           R"({"type":"server","protocol":"prg32-multiplayer-1.0","url":"http://10.0.0.2"})")
                           .isEmpty(),
                       "foreign scheme");
    failures += expect(Service::parseDiscovery("{not json").isEmpty(), "malformed");
    failures += expect(Service::parseDiscovery(good + QByteArray(512, ' ')).isEmpty(), "over 512 bytes");
    std::printf("%s: multiplayer wire checks, %d failed\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}

/** Frames run after the last scripted tap before the consoles are judged: 5 s at 30 frames per second. */
constexpr int SettleFrames = 150;
/** A loaded computer may stretch the run up to this multiple of the requested seconds. */
constexpr int HardLimitFactor = 3;
/** Frames a scripted tap is held: longer than one frame tick, so the press is observed and released. */
constexpr int TapFrames = 4;
/** The framebuffer's width in pixels (320 x 200, RGB565). */
constexpr int FramebufferWidth = 320;

/** Parse "A@60,DOWN+A@90" into the input mask to hold on each frame; an empty map on a syntax error. */
bool parseKeys(const QString& spec, QMap<int, uint32_t>& script) {
    static const QMap<QString, uint32_t> names = {{"LEFT", 1u << 0},
                                                  {"RIGHT", 1u << 1},
                                                  {"UP", 1u << 2},
                                                  {"DOWN", 1u << 3},
                                                  {"A", 1u << 4},
                                                  {"B", 1u << 5},
                                                  {"SELECT", 1u << 6},
                                                  {"START", 1u << 6},
                                                  {"C", 1u << 7},
                                                  {"D", 1u << 8},
                                                  {"L", 1u << 9},
                                                  {"R", 1u << 10}};
    for (const QString& tap : spec.split(',', Qt::SkipEmptyParts)) {
        const QStringList parts = tap.split('@');
        if (parts.size() != 2)
            return false;
        bool numeric = false;
        const int start = parts.at(1).toInt(&numeric);
        if (!numeric || start < 0)
            return false;
        uint32_t mask = 0;
        for (const QString& name : parts.at(0).toUpper().split('+')) {
            if (!names.contains(name))
                return false;
            mask |= names.value(name);
        }
        for (int frame = start; frame < start + TapFrames; ++frame)
            script[frame] |= mask;
    }
    return true;
}

/** Write an RGB565 framebuffer as a binary PPM (P6). */
void writePpm(const QString& path, const std::vector<uint16_t>& pixels) {
    std::ofstream out(path.toStdString(), std::ios::binary);
    const size_t height = pixels.size() / FramebufferWidth;
    out << "P6\n" << FramebufferWidth << ' ' << height << "\n255\n";
    for (const uint16_t pixel : pixels) {
        // RGB565: red in bits 15-11, green in 10-5, blue in 4-0, each widened to 8 bits.
        const uint8_t red = uint8_t(((pixel >> 11) & 0x1f) << 3);
        const uint8_t green = uint8_t(((pixel >> 5) & 0x3f) << 2);
        const uint8_t blue = uint8_t((pixel & 0x1f) << 3);
        out.put(char(red));
        out.put(char(green));
        out.put(char(blue));
    }
}

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
    if (argc == 2 && QLatin1String(argv[1]) == QLatin1String("--wire"))
        return wireChecks();
    QStringList positional;
    QMap<int, uint32_t> keyScript;
    QString dumpDirectory;
    for (int argument = 1; argument < argc; ++argument) {
        const QString value = QString::fromLocal8Bit(argv[argument]);
        if (value == QLatin1String("--keys") && argument + 1 < argc) {
            if (!parseKeys(QString::fromLocal8Bit(argv[++argument]), keyScript)) {
                std::fprintf(stderr, "invalid --keys; expected KEY[+KEY]@frame,...\n");
                return 2;
            }
        } else if (value == QLatin1String("--dump") && argument + 1 < argc) {
            dumpDirectory = QString::fromLocal8Bit(argv[++argument]);
        } else {
            positional << value;
        }
    }
    if (positional.isEmpty()) {
        std::fprintf(stderr,
                     "usage: prg32qt_multiplayer_check cartridge.prg32 [ws-url|discover] [consoles] [seconds]"
                     " [--keys KEY@frame,...] [--dump directory]\n"
                     "       prg32qt_multiplayer_check --wire\n");
        return 2;
    }
    QString url = positional.size() > 1 ? positional.at(1) : qEnvironmentVariable("PRG32QT_MULTIPLAYER_URL");
    if (url.isEmpty() || url == QLatin1String("-")) {
        std::printf("SKIP: set PRG32QT_MULTIPLAYER_URL to a PRG32 Multiplayer Server (ws://host:port/)\n");
        return 77;
    }
    const int consoles =
        std::clamp(positional.size() > 2 ? positional.at(2).toInt() : DefaultConsoles, 2, MaximumConsoles);
    const int seconds = std::max(1, positional.size() > 3 ? positional.at(3).toInt() : DefaultSeconds);

    std::ifstream file(positional.at(0).toStdString(), std::ios::binary);
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
        const bool discover = url == QLatin1String("discover");
        console.service->setLocalServerEnabled(true);
        console.service->setServerUrl(discover ? QString() : url);
        console.runtime = std::make_unique<prg32::Runtime>();
        console.runtime->setMultiplayerService(console.service.get());
        if (!console.runtime->load(*cartridge, error) || !console.runtime->init(error)) {
            std::fprintf(stderr, "FAIL: %s\n", error.c_str());
            return 1;
        }
    }

    bool failed = false;
    int frameNumber = 0;
    QTimer frames;
    QObject::connect(&frames, &QTimer::timeout, [&] {
        const uint32_t input = keyScript.value(frameNumber, 0);
        ++frameNumber;
        for (Console& console : fleet) {
            if (!console.runtime->frame(input, error)) {
                std::fprintf(stderr, "FAIL: frame: %s\n", error.c_str());
                failed = true;
                app.exit(1);
                return;
            }
        }
    });
    frames.start(FramePeriodMs);
    // Taps are numbered in frames, the run in seconds: on a loaded computer frames come late, so the run also
    // waits until the scripted keys have played and the lobby has had time to settle, up to a hard limit.
    const int lastKeyFrame = keyScript.isEmpty() ? 0 : keyScript.lastKey();
    QElapsedTimer runClock;
    runClock.start();
    QTimer finish;
    QObject::connect(&finish, &QTimer::timeout, [&] {
        const bool timeServed = runClock.elapsed() >= qint64(seconds) * 1000;
        const bool keysPlayed = frameNumber >= lastKeyFrame + SettleFrames;
        const bool limitReached = runClock.elapsed() >= qint64(seconds) * 1000 * HardLimitFactor;
        if ((timeServed && keysPlayed) || limitReached)
            app.exit(0);
    });
    finish.start(FramePeriodMs);
    if (app.exec() != 0 || failed)
        return 1;
    frames.stop();

    const bool discover = url == QLatin1String("discover");
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
        if (!dumpDirectory.isEmpty()) {
            QDir().mkpath(dumpDirectory);
            writePpm(QDir(dumpDirectory).filePath(QStringLiteral("console-%1.ppm").arg(index)),
                     fleet[index].runtime->framebuffer().rgb565Pixels());
        }
        const auto* qtService = static_cast<const QtMultiplayerService*>(fleet[index].service.get());
        std::printf("CONSOLE %zu player_id=%u peers=%d other_hosts=%zu non_black=%u server=%s%s\n",
                    index,
                    self,
                    count,
                    outsiders,
                    drawn,
                    qtService->currentServerIsLocal() ? "LOCAL " : "",
                    qPrintable(qtService->currentServerUrl()));
        pass =
            pass && std::includes(peers.begin(), peers.end(), expected.begin(), expected.end()) && drawn > 0;
        if (discover)
            pass = pass && qtService->currentServerIsLocal();
    }
    std::printf(
        "%s: %d consoles on %s, %d frames\n", pass ? "PASS" : "FAIL", consoles, qPrintable(url), frameNumber);
    return pass ? 0 : 1;
}
