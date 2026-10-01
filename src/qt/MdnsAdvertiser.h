#pragma once
#include <QObject>
#include <QString>
#include <memory>

/** Advertises the PRG32 HTTP device API through DNS-SD on the local network. */
class MdnsAdvertiser final : public QObject {
    Q_OBJECT
  public:
    /** Canonical DNS-SD service type consumed by PRG32 SDK device discovery. */
    static constexpr const char* ServiceType = "_prg32._tcp";

    /** Create and start a PRG32 device advertisement for the supplied TCP port. */
    explicit MdnsAdvertiser(quint16 port, QObject* parent = nullptr);
    ~MdnsAdvertiser() override;

    /** Return true when the platform backend successfully started advertising. */
    [[nodiscard]] bool active() const;

    /** Return the DNS-SD instance name requested from the platform backend. */
    [[nodiscard]] QString serviceName() const;

  private:
    class Backend;
    std::unique_ptr<Backend> backend_;
};
