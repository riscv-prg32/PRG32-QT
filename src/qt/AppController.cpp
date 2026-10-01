#include "AppController.h"
#include "FrameItem.h"
#include "GamepadBackend.h"
#include "InputButtons.h"
#include "QtAudioEngine.h"
#include "QtMultiplayerService.h"
#include "RiscVDisassembler.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <algorithm>
#include <cmath>
AppController::AppController(QObject* p)
    : QObject(p), audio_(std::make_unique<QtAudioEngine>()),
      multiplayer_(std::make_unique<QtMultiplayerService>()) {
    QSettings settings;
    preferredOrientation_ = settings.value("display/orientation", "auto").toString();
    if (preferredOrientation_ != "auto" && preferredOrientation_ != "portrait" &&
        preferredOrientation_ != "landscape") {
        preferredOrientation_ = "auto";
    }
    fullScreen_ = settings.value("display/fullScreen", false).toBool();
    statusBarsEnabled_ = settings.value("display/statusBars", false).toBool();
    debugEnabled_ = settings.value("debug/enabled", false).toBool();
    debugSpeed_ = settings.value("debug/speed", 1.0).toDouble();
    if (debugSpeed_ != 0.01 && debugSpeed_ != 0.025 && debugSpeed_ != 0.05 && debugSpeed_ != 0.1 &&
        debugSpeed_ != 0.25 && debugSpeed_ != 0.5 && debugSpeed_ != 1.0 && debugSpeed_ != 2.0 &&
        debugSpeed_ != 4.0)
        debugSpeed_ = 1.0;
    performanceMode_ = settings.value("performance/mode", "accurate").toString();
    if (performanceMode_ == "esp32-c6")
        performanceMode_ = "accurate";
    if (performanceMode_ != "accurate" && performanceMode_ != "optimal" && performanceMode_ != "unlimited") {
        performanceMode_ = "accurate";
    }
    prg32::PerformanceMode runtimeMode = prg32::PerformanceMode::Esp32C6Accurate;
    if (performanceMode_ == "optimal")
        runtimeMode = prg32::PerformanceMode::Optimal;
    else if (performanceMode_ == "unlimited")
        runtimeMode = prg32::PerformanceMode::Unlimited;
    rt_.setPerformanceMode(runtimeMode);
    rt_.setAudioSink(audio_.get());
    rt_.setMultiplayerService(multiplayer_.get());
    rt_.setLEDCallback([this](prg32::RGBState v) {
        led_ = v;
        emit ledChanged();
    });
    gamepad_ = createGamepadBackend(this);
    connect(gamepad_, &GamepadBackend::inputChanged, this, [this](quint32 mask) {
        input_.replace(InputState::Controller0, mask);
    });
    connect(gamepad_, &GamepadBackend::connectedChanged, this, &AppController::controllerChanged);
    timer_.setTimerType(Qt::PreciseTimer);
    updateTimerInterval();
    connect(&timer_, &QTimer::timeout, this, [this] {
        QElapsedTimer executionTimer;
        executionTimer.start();
        std::string e;
        bool frameComplete = false;
        if (debugEnabled_ && debugInstrumentation_) {
            debugFrameInput_ |= input_.takeMerged();
            static constexpr int DebugSlicesPerFrame = 8;
            const int instructionBudget = std::max(1, debugInstructionsPerFrame_ / DebugSlicesPerFrame);
            for (int instruction = 0; instruction < instructionBudget; ++instruction) {
                if (pauseAtBreakpoint())
                    return;
                if (!rt_.debugStep(debugFrameInput_, e)) {
                    timer_.stop();
                    running_ = false;
                    emit runningChanged();
                    setStatus(QString::fromStdString(e));
                    return;
                }
                ++debugInstructionsThisFrame_;
                if (rt_.debugPhase() == "idle") {
                    debugInstructionsPerFrame_ = std::max(1, debugInstructionsThisFrame_);
                    debugInstructionsThisFrame_ = 0;
                    debugFrameInput_ = 0;
                    frameComplete = true;
                    break;
                }
            }
            const auto pcIsInGuestMemory = [this] {
                const uint32_t pc = rt_.programCounter();
                return pc >= rt_.guestBase() &&
                       uint64_t(pc) < uint64_t(rt_.guestBase()) + rt_.guestMemorySize();
            };
            while (!frameComplete && !pcIsInGuestMemory()) {
                if (pauseAtBreakpoint())
                    return;
                if (!rt_.debugStep(debugFrameInput_, e)) {
                    timer_.stop();
                    running_ = false;
                    emit runningChanged();
                    setStatus(QString::fromStdString(e));
                    return;
                }
                ++debugInstructionsThisFrame_;
                if (rt_.debugPhase() == "idle") {
                    debugInstructionsPerFrame_ = std::max(1, debugInstructionsThisFrame_);
                    debugInstructionsThisFrame_ = 0;
                    debugFrameInput_ = 0;
                    frameComplete = true;
                }
            }
            updateFrame();
            lastExecutionMicroseconds_ = executionTimer.nsecsElapsed() / 1000;
            emit debugChanged();
            if (!frameComplete)
                return;
        } else if (!rt_.frame(input_.takeMerged(), e)) {
            timer_.stop();
            running_ = false;
            emit runningChanged();
            setStatus(QString::fromStdString(e));
            return;
        } else {
            frameComplete = true;
        }
        if (!frameComplete)
            return;
        ++frameCount_;
        ++frameRateCount_;
        const qint64 elapsedMilliseconds = frameRateTimer_.elapsed();
        if (elapsedMilliseconds >= 1000) {
            framesPerSecond_ = int(qint64(frameRateCount_) * 1000 / elapsedMilliseconds);
            frameRateCount_ = 0;
            frameRateTimer_.restart();
            emit frameStatsChanged();
        }
        led_.intensity *= .90;
        emit ledChanged();
        updateFrame();
        emit performanceChanged();
        lastExecutionMicroseconds_ = executionTimer.nsecsElapsed() / 1000;
        if (debugEnabled_)
            emit debugChanged();
    });
    ipTimer_.setInterval(5000);
    connect(&ipTimer_, &QTimer::timeout, this, &AppController::refreshIp);
    ipTimer_.start();
    refreshIp();
}
void AppController::setPerformanceMode(const QString& mode) {
    if (mode != "accurate" && mode != "optimal" && mode != "unlimited")
        return;
    if (performanceMode_ == mode)
        return;
    performanceMode_ = mode;
    QSettings().setValue("performance/mode", mode);
    prg32::PerformanceMode runtimeMode = prg32::PerformanceMode::Esp32C6Accurate;
    if (mode == "optimal")
        runtimeMode = prg32::PerformanceMode::Optimal;
    else if (mode == "unlimited")
        runtimeMode = prg32::PerformanceMode::Unlimited;
    rt_.setPerformanceMode(runtimeMode);
    updateTimerInterval();
    emit performanceModeChanged();
}
void AppController::setMultiplayerStoreUrl(const QUrl& url) {
    multiplayer_->setStoreUrl(url);
}
void AppController::setPreferredOrientation(const QString& orientation) {
    if (orientation != "auto" && orientation != "portrait" && orientation != "landscape")
        return;
    if (preferredOrientation_ == orientation)
        return;
    preferredOrientation_ = orientation;
    QSettings().setValue("display/orientation", orientation);
    emit displayPreferencesChanged();
}
void AppController::setFullScreen(bool enabled) {
    if (fullScreen_ == enabled)
        return;
    fullScreen_ = enabled;
    QSettings().setValue("display/fullScreen", enabled);
    emit displayPreferencesChanged();
}
void AppController::setStatusBarsEnabled(bool enabled) {
    if (statusBarsEnabled_ == enabled)
        return;
    statusBarsEnabled_ = enabled;
    QSettings().setValue("display/statusBars", enabled);
    updateFrame();
    emit displayPreferencesChanged();
}
AppController::~AppController() {
    rt_.stop();
}
void AppController::setStatus(QString s) {
    if (status_ == s)
        return;
    status_ = std::move(s);
    emit statusChanged();
}
bool AppController::controllerConnected() const {
    return gamepad_ && gamepad_->connected();
}
QString AppController::controllerName() const {
    return gamepad_ ? gamepad_->name() : QString();
}
void AppController::updateFrame() {
    if (frame_)
        frame_->setFrame(rt_.framebuffer().rgb565Pixels());
}
bool AppController::loadBytes(const QByteArray& d, const QString& suggestedName) {
    std::string e;
    auto c =
        prg32::Cartridge::parse(std::span<const uint8_t>((const uint8_t*)d.constData(), size_t(d.size())), e);
    if (!c) {
        setStatus(QString::fromStdString(e));
        return false;
    }
    timer_.stop();
    rt_.stop();
    if (!rt_.load(*c, e) || !rt_.init(e)) {
        setStatus(QString::fromStdString(e));
        return false;
    }
    currentBytes_ = d;
    cartridgeName_ = QString::fromStdString(c->name());
    auto meta = QJsonDocument::fromJson(QByteArray::fromStdString(c->metadataJson())).object();
    auto tags = meta.value("tags").toArray();
    performanceAvailable_ = meta.contains("performance_contract") || meta.contains("performance") ||
                            std::any_of(tags.begin(), tags.end(), [](const QJsonValue& v) {
                                return v.toString().compare("benchmark", Qt::CaseInsensitive) == 0 ||
                                       v.toString().compare("performance", Qt::CaseInsensitive) == 0;
                            });
    frameCount_ = 0;
    debugFrameInput_ = 0;
    debugInstructionsThisFrame_ = 0;
    debugInstructionsPerFrame_ = 8192;
    debugViewAddress_ = 0;
    debugView_ = "pc";
    debugBreakpoints_.clear();
    ignoreBreakpointOnce_ = false;
    frameRateCount_ = 0;
    framesPerSecond_ = 0;
    frameRateTimer_.restart();
    emit frameStatsChanged();
    emit cartridgeChanged();
    emit performanceChanged();
    updateFrame();
    if (debugEnabled_)
        inspectMemory(QString("0x%1").arg(rt_.guestBase(), 8, 16, QChar('0')), 128);
    timer_.start();
    running_ = true;
    paused_ = false;
    emit runningChanged();
    emit pausedChanged();
    QString n = suggestedName.isEmpty() ? cartridgeName_ : suggestedName;
    saveCartridge(d, n);
    setStatus(QString("Running %1").arg(cartridgeName_));
    return true;
}
bool AppController::loadFile(const QUrl& u) {
    QString path = u.toLocalFile();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setStatus("Cannot open cartridge file");
        return false;
    }
    return loadBytes(f.readAll(), QFileInfo(path).completeBaseName());
}
void AppController::saveCartridge(const QByteArray& d, const QString& name) {
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty())
        return;
    QDir dir(root);
    dir.mkpath("PRG32Cartridges");
    QString safe = name;
    safe.replace('/', '_').replace('\\', '_');
    QFile f(dir.filePath("PRG32Cartridges/" + safe + ".prg32"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(d);
}
void AppController::stop() {
    timer_.stop();
    rt_.stop();
    input_ = InputState{};
    paused_ = false;
    emit pausedChanged();
    if (running_) {
        running_ = false;
        emit runningChanged();
    }
}
void AppController::pause() {
    if (!running_ || paused_)
        return;
    timer_.stop();
    rt_.pause();
    input_ = InputState{};
    paused_ = true;
    emit pausedChanged();
    setStatus(QString("Paused %1").arg(cartridgeName_));
}
void AppController::resume() {
    if (!running_ || !paused_)
        return;
    ignoreBreakpointOnce_ = debugBreakpoints_.contains(debugExecutionAddress());
    paused_ = false;
    timer_.start();
    emit pausedChanged();
    setStatus(QString("Running %1").arg(cartridgeName_));
}
void AppController::setDebugEnabled(bool enabled) {
    if (debugEnabled_ == enabled)
        return;
    debugEnabled_ = enabled;
    QSettings().setValue("debug/enabled", enabled);
    updateTimerInterval();
    if (enabled && rt_.loaded())
        inspectMemory(QString("0x%1").arg(rt_.guestBase(), 8, 16, QChar('0')), 128);
    emit debugChanged();
}
void AppController::setDebugInstrumentation(bool enabled) {
    if (debugInstrumentation_ == enabled)
        return;
    debugInstrumentation_ = enabled;
    updateTimerInterval();
    emit debugChanged();
}
void AppController::setDebugSpeed(double speed) {
    static constexpr double SupportedSpeeds[] = {0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 4.0};
    const auto supported = std::find(std::begin(SupportedSpeeds), std::end(SupportedSpeeds), speed);
    if (supported == std::end(SupportedSpeeds) || debugSpeed_ == speed)
        return;
    debugSpeed_ = speed;
    QSettings().setValue("debug/speed", speed);
    updateTimerInterval();
    emit debugChanged();
}
uint32_t AppController::debugExecutionAddress() const {
    if (rt_.debugCallActive())
        return rt_.programCounter();
    if (!rt_.loaded())
        return 0;
    return rt_.guestBase() + rt_.cartridge().header().updateOffset;
}
bool AppController::pauseAtBreakpoint() {
    const uint32_t address = debugExecutionAddress();
    if (!debugBreakpoints_.contains(address))
        return false;
    if (ignoreBreakpointOnce_) {
        ignoreBreakpointOnce_ = false;
        return false;
    }
    debugViewAddress_ = 0;
    debugView_ = "pc";
    pause();
    setStatus(QString("Breakpoint at 0x%1").arg(address, 8, 16, QChar('0')));
    emit debugChanged();
    return true;
}
QString AppController::debugBreakpoints() const {
    QList<uint32_t> addresses = debugBreakpoints_.values();
    std::sort(addresses.begin(), addresses.end());
    QStringList labels;
    for (const uint32_t address : addresses)
        labels.append(QString("0x%1").arg(address, 8, 16, QChar('0')));
    return labels.isEmpty() ? "None" : labels.join("  ");
}
bool AppController::toggleBreakpoint(const QString& addressText) {
    bool ok = false;
    QString normalized = addressText.trimmed();
    int base = 10;
    if (normalized.startsWith("0x", Qt::CaseInsensitive)) {
        normalized.remove(0, 2);
        base = 16;
    }
    const uint32_t address = normalized.toUInt(&ok, base);
    if (!ok || rt_.debugMemory(address, 2).size() != 2) {
        setStatus("Invalid breakpoint address");
        return false;
    }
    if (debugBreakpoints_.contains(address))
        debugBreakpoints_.remove(address);
    else
        debugBreakpoints_.insert(address);
    emit debugChanged();
    return true;
}
void AppController::updateTimerInterval() {
    if (debugEnabled_ && debugInstrumentation_) {
        static constexpr double DebugSlicesPerFrame = 8.0;
        timer_.setInterval(std::max(1, int(std::lround(33.0 / (debugSpeed_ * DebugSlicesPerFrame)))));
        return;
    }
    timer_.setInterval(performanceMode_ == "unlimited" ? 0 : 33);
}
QString AppController::telemetryInputMask() const {
    return QString("0x%1").arg(input_.merged() | debugFrameInput_, 8, 16, QChar('0'));
}
static QVariantList waveformVariant(const std::vector<float>& samples) {
    QVariantList values;
    values.reserve(qsizetype(samples.size()));
    for (const float sample : samples)
        values.append(sample);
    return values;
}
QVariantList AppController::telemetryWaveformLeft() const {
    return waveformVariant(audio_->waveformLeft());
}
QVariantList AppController::telemetryWaveformRight() const {
    return waveformVariant(audio_->waveformRight());
}
QString AppController::telemetryPerformance() const {
    return QString("%1 FPS · %2 µs host · %3 insn · %4 cycles · %5 late")
        .arg(framesPerSecond_)
        .arg(lastExecutionMicroseconds_)
        .arg(rt_.retiredInstructions())
        .arg(rt_.virtualCycles())
        .arg(rt_.lateFrames());
}
bool AppController::debugStep() {
    if (!debugEnabled_ || !running_)
        return false;
    if (!paused_)
        pause();
    debugViewAddress_ = 0;
    debugView_ = "pc";
    std::string error;
    if (!rt_.debugStep(input_.takeMerged(), error)) {
        setStatus(QString::fromStdString(error));
        return false;
    }
    updateFrame();
    emit debugChanged();
    return true;
}
void AppController::showDebugEntry(const QString& entry) {
    if (!rt_.loaded())
        return;
    const auto& header = rt_.cartridge().header();
    if (entry == "init")
        debugViewAddress_ = rt_.guestBase() + header.initOffset;
    else if (entry == "update")
        debugViewAddress_ = rt_.guestBase() + header.updateOffset;
    else if (entry == "draw")
        debugViewAddress_ = rt_.guestBase() + header.drawOffset;
    else {
        debugViewAddress_ = 0;
        debugView_ = "pc";
        emit debugChanged();
        return;
    }
    debugView_ = entry;
    emit debugChanged();
}
void AppController::inspectMemory(const QString& addressText, int length) {
    bool ok = false;
    QString normalized = addressText.trimmed();
    int base = 10;
    if (normalized.startsWith("0x", Qt::CaseInsensitive)) {
        normalized.remove(0, 2);
        base = 16;
    }
    const uint32_t address = normalized.toUInt(&ok, base);
    if (!ok) {
        debugMemoryError_ = "Invalid address";
        emit debugChanged();
        return;
    }
    const auto bytes = rt_.debugMemory(address, 1);
    if (bytes.empty()) {
        debugMemoryError_ = "Address outside guest memory";
        emit debugChanged();
        return;
    }
    debugMemoryError_.clear();
    debugMemoryAddress_ = address;
    debugMemoryLength_ = std::clamp(length, 1, 1024);
    emit debugChanged();
}
QString AppController::debugMemory() const {
    if (!debugMemoryError_.isEmpty())
        return debugMemoryError_;
    const auto bytes = rt_.debugMemory(debugMemoryAddress_, size_t(debugMemoryLength_));
    QStringList lines;
    for (size_t offset = 0; offset < bytes.size(); offset += 16) {
        QString line = QString("%1  ").arg(uint64_t(debugMemoryAddress_) + offset, 8, 16, QChar('0'));
        QString ascii;
        for (size_t column = 0; column < 16; ++column) {
            if (offset + column < bytes.size()) {
                const uint8_t value = bytes[offset + column];
                line += QString("%1 ").arg(value, 2, 16, QChar('0'));
                ascii += value >= 32 && value < 127 ? QChar(value) : QChar('.');
            } else {
                line += "   ";
                ascii += ' ';
            }
        }
        lines.append(line + " |" + ascii + '|');
    }
    return lines.isEmpty() ? "Address outside guest memory" : lines.join('\n');
}
QString AppController::debugPhase() const {
    return QString::fromStdString(rt_.debugPhase());
}
QString AppController::debugRegisters() const {
    static const char* names[] = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
                                  "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                                  "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    QStringList lines;
    for (unsigned index = 0; index < 32; index += 4) {
        QStringList row;
        for (unsigned column = 0; column < 4; ++column) {
            const unsigned reg = index + column;
            row.append(QString("%1 %2").arg(names[reg], -4).arg(rt_.registerValue(reg), 8, 16, QChar('0')));
        }
        lines.append(row.join("  "));
    }
    return lines.join('\n');
}
QVariantList AppController::debugAssemblyRows() const {
    QVariantList rows;
    if (!rt_.loaded())
        return rows;
    const auto trace = rt_.recentInstructionAddresses();
    const uint32_t currentAddress = rt_.debugCallActive()
                                        ? rt_.programCounter()
                                        : rt_.guestBase() + rt_.cartridge().header().updateOffset;
    uint32_t address = debugViewAddress_ ? debugViewAddress_ : currentAddress;
    if (address < rt_.guestBase() || uint64_t(address) >= uint64_t(rt_.guestBase()) + rt_.guestMemorySize())
        address = rt_.guestBase() + rt_.cartridge().header().updateOffset;
    const auto appendInstruction = [&](uint32_t cursor, const QString& state) -> unsigned {
        const auto first = rt_.debugMemory(cursor, 2);
        if (first.size() != 2)
            return 0;
        const uint16_t low = uint16_t(first[0]) | uint16_t(first[1]) << 8;
        const unsigned width = (low & 3u) == 3u ? 4u : 2u;
        const auto bytes = rt_.debugMemory(cursor, width);
        if (bytes.size() != width)
            return 0;
        uint32_t instruction = low;
        if (width == 4)
            instruction |= uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
        const QString assembly = QString::fromStdString(prg32::disassembleRv32(cursor, instruction, width));
        const int separator = assembly.indexOf(' ');
        const QString operation = separator < 0 ? assembly : assembly.left(separator);
        const QString operands = separator < 0 ? QString() : assembly.mid(separator);
        QVariantMap row;
        row.insert("address", QString("%1").arg(cursor, 8, 16, QChar('0')));
        row.insert("operation", operation);
        row.insert("operands", operands);
        row.insert("current", state == "current");
        row.insert("breakpoint", debugBreakpoints_.contains(cursor));
        rows.append(row);
        return width;
    };
    if (debugView_ == "pc") {
        const int firstPast = std::max(0, int(trace.size()) - 6);
        for (int index = firstPast; index < int(trace.size()); ++index) {
            if (trace[size_t(index)] != currentAddress)
                appendInstruction(trace[size_t(index)], "past");
        }
        unsigned width = appendInstruction(currentAddress, "current");
        uint32_t cursor = currentAddress + width;
        while (width && rows.size() < 15) {
            width = appendInstruction(cursor, "next");
            cursor += width;
        }
        return rows;
    }
    uint32_t cursor = address;
    for (int line = 0; line < 15; ++line) {
        const bool past = std::find(trace.begin(), trace.end(), cursor) != trace.end();
        const QString state = cursor == currentAddress ? "current" : (past ? "past" : "next");
        const unsigned width = appendInstruction(cursor, state);
        if (!width)
            break;
        cursor += width;
    }
    return rows;
}
QString AppController::debugAssembly() const {
    const QVariantList rows = debugAssemblyRows();
    if (rows.isEmpty())
        return "<span style='color:#77808d'>No cartridge loaded</span>";
    QStringList lines;
    for (const QVariant& value : rows) {
        const QVariantMap row = value.toMap();
        const bool current = row.value("current").toBool();
        const bool breakpoint = row.value("breakpoint").toBool();
        const QString marker = current ? "▶" : (breakpoint ? "●" : " ");
        const QString background = current ? "background-color:#075985;color:#ffffff;font-weight:700;" : "";
        const QString markerColor = breakpoint && !current ? "#ff3b30" : "#45c9ff";
        lines.append(
            QString(
                "<div style='%1'><span style='color:%2'>%3 %4</span>  <span "
                "style='color:#d987ff;font-weight:600'>%5</span><span style='color:#d6dbe3'>%6</span></div>")
                .arg(background,
                     markerColor,
                     marker,
                     row.value("address").toString(),
                     row.value("operation").toString().toHtmlEscaped(),
                     row.value("operands").toString().toHtmlEscaped()));
    }
    return lines.join(QString());
}
void AppController::setButton(int m, bool down) {
    input_.set(InputState::Ui, uint32_t(m), down);
}
void AppController::setDirectional(int m) {
    input_.replace(InputState::Ui,
                   (input_.value(InputState::Ui) & ~prg32qt::input::DirectionMask) |
                       (uint32_t(m) & prg32qt::input::DirectionMask));
}
void AppController::setKeyboardButton(int m, bool d) {
    input_.set(InputState::Keyboard, uint32_t(m), d);
}
void AppController::clearKeyboard() {
    input_.clear(InputState::Keyboard);
}
void AppController::attachFrame(QObject* o) {
    frame_ = qobject_cast<FrameItem*>(o);
    updateFrame();
}
void AppController::playStartupTone() {
    audio_->tone(523.25, 120, 180);
    QTimer::singleShot(130, this, [this] { audio_->tone(659.25, 120, 180); });
    QTimer::singleShot(260, this, [this] { audio_->tone(783.99, 660, 180); });
}
void AppController::refreshIp() {
    QString chosen;
    for (const auto& i : QNetworkInterface::allInterfaces()) {
        if (!(i.flags() & QNetworkInterface::IsUp) || !(i.flags() & QNetworkInterface::IsRunning) ||
            (i.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const auto& a : i.addressEntries()) {
            auto ip = a.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback())
                continue;
            QString candidate = ip.toString();
            if (chosen.isEmpty())
                chosen = candidate;
            if (i.humanReadableName().contains("Wi-Fi", Qt::CaseInsensitive) || i.name() == "en0") {
                chosen = candidate;
                break;
            }
        }
    }
    if (deviceIp_ != chosen) {
        deviceIp_ = chosen;
        emit networkChanged();
    }
}
QString AppController::slotPath(int slot) const {
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(root).filePath(QString("PRG32Cartridges/cart%1.prg32").arg(slot));
}
QString AppController::performanceState() const {
    switch (rt_.performance().state) {
    case 1:
        return "Running";
    case 2:
        return "Complete";
    case 3:
        return "Aborted";
    default:
        return "Idle";
    }
}
void AppController::runPerformanceTest() {
    if (!performanceAvailable_ || currentBytes_.isEmpty())
        return;
    auto bytes = currentBytes_;
    auto name = cartridgeName_;
    if (loadBytes(bytes, name))
        setStatus("Performance test running — results at /api/performance.json");
}
QJsonObject AppController::runtimeJson() const {
    auto& h = rt_.cartridge().header();
    const uint32_t cartridgeLoadAddress = h.loadAddr ? h.loadAddr : 0x40800000u;
    QJsonObject cart{
        {"name", cartridgeName_},
        {"loaded", rt_.loaded()},
        {"stored", !currentBytes_.isEmpty()},
        {"code_size", int(h.codeSize)},
        {"mem_size", int(h.memSize)},
        {"audio_size", rt_.cartridge().audio() ? int(rt_.cartridge().audio()->samples.size()) : 0},
        {"audio", bool(rt_.cartridge().audio())},
        {"generation", int(frameCount_ ? 1 : 0)}};
    return {
        {"name", "PRG32"},
        {"firmware_version", "qt-" PRG32QT_VERSION},
        {"cart_magic", "PRG2"},
        {"cart_abi_major", 1},
        {"cart_abi_minor", 6},
        {"cart_abi_hash", double(prg32::Runtime::CurrentAbiHash)},
        {"cart_abi_features", double(prg32::Runtime::ProvidedFeatures)},
        {"cart_load_addr", double(cartridgeLoadAddress)},
        {"cart_max_size", double(prg32::Cartridge::MaximumGuestBytes)},
        {"cart_ram_size", double(prg32::Cartridge::MaximumGuestBytes)},
        {"cart_loaded", rt_.loaded()},
        {"qemu", false},
        {"cart", cart},
        {"diag", QJsonObject{{"frame_count", double(frameCount_)}, {"input_state", double(input_.merged())}}},
        {"host", "qt"}};
}
QJsonArray AppController::gamesJson() const {
    QJsonArray out;
    for (int i = 0; i < 4; ++i) {
        QFile f(slotPath(i));
        QJsonObject g{{"slot", QString("cart%1").arg(i)},
                      {"name", ""},
                      {"loaded", false},
                      {"stored", false},
                      {"code_size", 0},
                      {"mem_size", 0},
                      {"audio_size", 0},
                      {"audio", false},
                      {"generation", 0}};
        if (f.open(QIODevice::ReadOnly)) {
            std::string e;
            auto b = f.readAll();
            auto c = prg32::Cartridge::parse(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(b.constData()), size_t(b.size())),
                e);
            if (c) {
                g["name"] = QString::fromStdString(c->name());
                g["stored"] = true;
                g["loaded"] = currentBytes_ == b;
                g["code_size"] = int(c->header().codeSize);
                g["mem_size"] = int(c->header().memSize);
                g["audio"] = bool(c->audio());
                g["audio_size"] = c->audio() ? int(c->audio()->samples.size()) : 0;
                g["generation"] = currentBytes_ == b ? 1 : 0;
            }
        }
        out.append(g);
    }
    return out;
}
bool AppController::uploadSlot(int slot, const QByteArray& bytes, QString& error) {
    if (slot < 0 || slot >= 4) {
        error = "invalid cartridge slot";
        return false;
    }
    std::string e;
    auto c = prg32::Cartridge::parse(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.constData()), size_t(bytes.size())),
        e);
    if (!c) {
        error = QString::fromStdString(e);
        return false;
    }
    QDir().mkpath(QFileInfo(slotPath(slot)).absolutePath());
    QSaveFile file(slotPath(slot));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = "cannot store cartridge";
        return false;
    }
    return true;
}
bool AppController::selectSlot(int slot, QString& error) {
    if (slot < 0 || slot >= 4) {
        error = "invalid cartridge slot";
        return false;
    }
    QFile file(slotPath(slot));
    if (!file.open(QIODevice::ReadOnly)) {
        error = "slot is empty";
        return false;
    }
    if (!loadBytes(file.readAll(), QString("cart%1").arg(slot))) {
        error = status_;
        return false;
    }
    return true;
}
QJsonObject AppController::memoryJson() const {
    auto m = rt_.loaded() ? rt_.cartridge().header().memSize : 0;
    return {{"static_bss_bytes", 0},
            {"static_data_bytes", 0},
            {"heap_total_bytes", double(m)},
            {"heap_free_bytes", 0},
            {"heap_allocated_bytes", double(m)},
            {"heap_largest_free_block", 0},
            {"host", "qt"}};
}
QJsonObject AppController::debugJson(uint32_t address, int length) const {
    QJsonArray registers;
    for (unsigned index = 0; index < 32; ++index)
        registers.append(double(rt_.registerValue(index)));
    const uint32_t pc = debugExecutionAddress();
    if (address == 0)
        address = pc >= rt_.guestBase() && uint64_t(pc) < uint64_t(rt_.guestBase()) + rt_.guestMemorySize()
                      ? pc
                      : rt_.guestBase();
    const auto bytes = rt_.debugMemory(address, size_t(std::clamp(length, 1, 1024)));
    QByteArray raw(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
    QJsonArray breakpoints;
    QList<uint32_t> breakpointAddresses = debugBreakpoints_.values();
    std::sort(breakpointAddresses.begin(), breakpointAddresses.end());
    for (const uint32_t breakpointAddress : breakpointAddresses)
        breakpoints.append(double(breakpointAddress));
    return {{"ok", true},
            {"enabled", debugEnabled_},
            {"running", running_ && !paused_},
            {"paused", paused_},
            {"phase", debugPhase()},
            {"speed", debugSpeed_},
            {"pc", double(pc)},
            {"breakpoints", breakpoints},
            {"assembly_html", debugAssembly()},
            {"registers", registers},
            {"memory_address", double(address)},
            {"memory_hex", QString::fromLatin1(raw.toHex())}};
}
bool AppController::debugCommand(const QJsonObject& command, QString& error) {
    const QString operation = command.value("command").toString().toLower();
    if (operation == "enable") {
        setDebugEnabled(command.value("enabled").toBool(true));
        return true;
    }
    if (operation == "speed") {
        const double requestedSpeed = command.value("speed").toDouble(-1.0);
        const double previousSpeed = debugSpeed_;
        setDebugSpeed(requestedSpeed);
        if (debugSpeed_ != requestedSpeed && previousSpeed != requestedSpeed) {
            error = "supported speeds are 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2, and 4";
            return false;
        }
        return true;
    }
    if (!debugEnabled_) {
        error = "debug mode is disabled";
        return false;
    }
    if (operation == "breakpoint") {
        bool ok = false;
        uint32_t address = 0;
        const QJsonValue addressValue = command.value("address");
        if (addressValue.isString()) {
            QString normalized = addressValue.toString().trimmed();
            int base = 10;
            if (normalized.startsWith("0x", Qt::CaseInsensitive)) {
                normalized.remove(0, 2);
                base = 16;
            }
            address = normalized.toUInt(&ok, base);
        } else if (addressValue.isDouble()) {
            const double numericAddress = addressValue.toDouble(-1.0);
            ok = numericAddress >= 0.0 && numericAddress <= double(UINT32_MAX);
            address = ok ? uint32_t(numericAddress) : 0;
        }
        if (!ok || rt_.debugMemory(address, 2).size() != 2) {
            error = "invalid breakpoint address";
            return false;
        }
        if (command.value("enabled").toBool(true))
            debugBreakpoints_.insert(address);
        else
            debugBreakpoints_.remove(address);
        emit debugChanged();
        return true;
    }
    if (operation == "pause") {
        if (!running_) {
            error = "no cartridge is running";
            return false;
        }
        pause();
        return true;
    }
    if (operation == "resume") {
        if (!running_) {
            error = "no cartridge is running";
            return false;
        }
        resume();
        return true;
    }
    if (operation == "step") {
        if (debugStep())
            return true;
        error = status_.isEmpty() ? "unable to step guest execution" : status_;
        return false;
    }
    error = "expected command enable, pause, step, or resume";
    return false;
}
QJsonArray AppController::scoresJson() const {
    QJsonArray a;
    for (const auto& s : rt_.scores())
        a.append(QJsonObject{{"game", QString::fromStdString(s.game)},
                             {"player", QString::fromStdString(s.player)},
                             {"score", double(s.score)}});
    return a;
}
bool AppController::submitScore(const QJsonObject& o, QString& error) {
    auto g = o.value("game").toString(), p = o.value("player").toString();
    double score = o.value("score").toDouble(-1);
    if (g.isEmpty() || p.isEmpty() || score < 0 || score > UINT32_MAX) {
        error = "expected game, player, score";
        return false;
    }
    rt_.submitScore(g.toStdString(), p.toStdString(), uint32_t(score));
    return true;
}
QByteArray AppController::screenshotBmp() const {
    constexpr int w = 320, h = 200, row = w * 3;
    QByteArray b(54 + row * h, 0);
    auto p = reinterpret_cast<unsigned char*>(b.data());
    auto wr16 = [&](int o, uint16_t v) {
        p[o] = uint8_t(v);
        p[o + 1] = uint8_t(v >> 8);
    };
    auto wr32 = [&](int o, uint32_t v) {
        for (int i = 0; i < 4; ++i)
            p[o + i] = uint8_t(v >> (i * 8));
    };
    p[0] = 'B';
    p[1] = 'M';
    wr32(2, uint32_t(b.size()));
    wr32(10, 54);
    wr32(14, 40);
    wr32(18, w);
    wr32(22, h);
    wr16(26, 1);
    wr16(28, 24);
    wr32(34, row * h);
    const auto& pixels = rt_.framebuffer().rgb565Pixels();
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint16_t c = pixels[size_t(y) * w + x];
            int o = 54 + (h - 1 - y) * row + x * 3;
            p[o] = uint8_t((c & 31) * 255 / 31);
            p[o + 1] = uint8_t(((c >> 5) & 63) * 255 / 63);
            p[o + 2] = uint8_t(((c >> 11) & 31) * 255 / 31);
        }
    return b;
}
QJsonObject AppController::performanceJson() const {
    const auto& s = rt_.performance();
    if (s.state == 1)
        return {{"ok", false}, {"running", true}};
    if (s.state != 2)
        return {{"ok", false},
                {"error", "no performance test results"},
                {"performance_mode", performanceMode_},
                {"virtual_clock_hz", double(prg32::VirtualClock::FrequencyHz)},
                {"late_frames", double(rt_.lateFrames())}};
    QJsonArray cases;
    uint64_t total = 0, updates = 0, draws = 0, presents = 0;
    uint32_t frames = 0, min = UINT32_MAX, max = 0, missed = 0, screenCount = 0;
    for (const auto& c : s.cases) {
        cases.append(QJsonObject{{"screen_index", int(c.index)},
                                 {"screen_name", QString::fromStdString(c.name)},
                                 {"color_mode",
                                  c.mode == 0   ? "rgb565"
                                  : c.mode == 1 ? "indexed"
                                                : "custom"},
                                 {"metric_goal", QString::fromStdString(c.goal)},
                                 {"first_frame", int(c.first)},
                                 {"last_frame", int(c.first + c.frames - 1)},
                                 {"frames", int(c.frames)},
                                 {"fps_mean", c.mean ? 1000000.0 / double(c.mean) : 0.0},
                                 {"frame_us_min", int(c.min)},
                                 {"frame_us_mean", int(c.mean)},
                                 {"frame_us_p50", int(c.p50)},
                                 {"frame_us_p95", int(c.p95)},
                                 {"frame_us_p99", int(c.p99)},
                                 {"frame_us_max", int(c.max)},
                                 {"missed_deadlines", int(c.missed)},
                                 {"update_us_mean", int(c.update)},
                                 {"draw_us_mean", int(c.draw)},
                                 {"present_us_mean", int(c.present)},
                                 {"heap_min", 0}});
        frames += c.frames;
        total += c.frameTotal;
        updates += c.updateTotal;
        draws += c.drawTotal;
        presents += c.presentTotal;
        min = std::min(min, c.min);
        max = std::max(max, c.max);
        missed += c.missed;
        screenCount = std::max(screenCount, c.index + 1);
    }
    double mean = frames ? double(total) / frames : 0;
    QJsonObject summary{{"frames", int(frames)},
                        {"fps_mean", mean ? 1000000.0 / mean : 0.0},
                        {"frame_us_min", int(min == UINT32_MAX ? 0 : min)},
                        {"frame_us_mean", mean},
                        {"frame_us_max", int(max)},
                        {"missed_deadlines", int(missed)},
                        {"update_us_mean", frames ? double(updates) / frames : 0.0},
                        {"draw_us_mean", frames ? double(draws) / frames : 0.0},
                        {"present_us_mean", frames ? double(presents) / frames : 0.0},
                        {"heap_min", 0},
                        {"screen_count", int(screenCount)}};
    return {{"ok", true},
            {"schema_version", 2},
            {"performance_mode", performanceMode_},
            {"virtual_clock_hz", double(prg32::VirtualClock::FrequencyHz)},
            {"late_frames", double(rt_.lateFrames())},
            {"run_id", QString("perf-%1-%2").arg(s.started).arg(s.sequence)},
            {"board_id", "qt-host"},
            {"target", "qt"},
            {"display_backend", "qt-quick"},
            {"game_name", QString::fromStdString(s.name)},
            {"wifi_mode", deviceIp_.isEmpty() ? "off" : "infrastructure"},
            {"sample_period_frames", 0},
            {"screen_count", int(screenCount)},
            {"result_count", int(s.cases.size())},
            {"color_modes", QJsonArray{"rgb565", "indexed"}},
            {"started_at_device_us", double(s.started)},
            {"started_at_server_ts", QJsonValue::Null},
            {"duration_us", double(s.ended - s.started)},
            {"samples", QJsonArray{}},
            {"aggregate_windows", QJsonArray{}},
            {"screen_summaries", cases},
            {"comparisons", QJsonArray{}},
            {"memory",
             QJsonObject{{"baseline_free_bytes", 0},
                         {"baseline_largest_block", 0},
                         {"peak_free_bytes", 0},
                         {"peak_largest_block", 0},
                         {"after_free_bytes", 0},
                         {"after_largest_block", 0}}},
            {"summary", summary}};
}
