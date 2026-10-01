# Network discovery and PRG32 SDK access

PRG32-QT exposes the same PRG32 device HTTP contract used to deploy, select, run, and inspect cartridges on the
ESP32-C6 firmware and QEMU. The application listens on TCP port 8080 and advertises that listener on the local
link with Bonjour/mDNS, so tooling does not need a manually entered numeric IP address.

## DNS-SD contract

| Field | Value |
|---|---|
| Service type | `_prg32._tcp.local.` |
| Transport | TCP |
| Default port | `8080` |
| TXT `api` | `prg32-http-1` |
| TXT `path` | `/api` |
| TXT `runtime` | `qt` |
| TXT `version` | Derived PRG32-QT application version |

On the wire, the fixed TXT entries are `api=prg32-http-1`, `path=/api`, and `runtime=qt`; `version=` carries the
derived build version.

The DNS-SD instance name is `PRG32-QT on <host>`. A client must use the SRV target and port returned by DNS-SD
rather than constructing a hostname or assuming the default port. Service names may be renamed automatically
when Bonjour detects another instance with the same name.

The advertisement is link-local. Routers do not normally forward mDNS between VLANs, Wi-Fi client-isolation
domains, VPNs, or routed subnets. Discovery also does not replace IP connectivity: host firewalls must permit
inbound mDNS UDP port 5353 and the advertised API TCP port.

## PRG32 SDK workflow

After a DNS-SD browser resolves an instance, use its URL with the existing SDK commands. For example, if
Bonjour resolves the service to `prg32-host.local.:8080`:

```sh
python3 -m prg32 runtime --url http://prg32-host.local:8080
python3 -m prg32 esp32c6 upload-and-run game.prg32 \
  --url http://prg32-host.local:8080 --slot cart0
```

The SDK currently accepts a resolved URL for its device operations. DNS-SD-capable IDEs and SDK frontends can
browse `_prg32._tcp.local.`, validate `api=prg32-http-1`, and pass the resolved URL to those operations. The
service does not advertise the Cartridge Store: Store discovery uses the separate `_prg32store._tcp.local.`
contract.

Before any cartridge is loaded, `/api/runtime` reports the default load address and total available guest RAM.
This lets the SDK validate the first upload rather than incorrectly treating an idle runtime as having zero
cartridge memory.

## Device HTTP API

The discovery document at `GET /api` lists the active endpoints. The current host provides:

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api` or `/api/` | API discovery |
| `GET` | `/api/runtime` | ABI, capacity, active cartridge, frame and input state |
| `GET` | `/api/games` | Four persistent cartridge slots |
| `POST` | `/api/games?slot=cartN` | Upload a cartridge to `cart0` through `cart3` |
| `POST` | `/api/games/select?slot=cartN` | Load and run a stored cartridge |
| `GET` | `/api/screenshot.bmp` | Current framebuffer capture |
| `GET` | `/api/performance.json` | Performance-contract state and results |
| `GET` | `/api/memory` | Runtime memory statistics |
| `GET` | `/api/scores` | Local scores |
| `POST` | `/api/scores` | Submit a local score |

Uploads still pass the normal PRG2 bounds, CRC, ABI, feature, import-model, and memory validation. Discovery does
not weaken cartridge isolation or execute native host code.

## Platform implementation

- macOS, iOS, and Apple TV register with the system DNS-SD/Bonjour API. The mobile bundle manifests declare
  `_prg32._tcp` and explain local-network access to the user.
- Windows, Linux, Raspberry Pi OS, Android, and Android TV use the dependency-free Qt UDP responder. It joins
  the IPv4 mDNS group on each active multicast interface, answers PTR/SRV/TXT/A and ANY questions, refreshes
  records, and sends zero-TTL goodbye records at shutdown.
- Android packages request `CHANGE_WIFI_MULTICAST_STATE`; PRG32-QT holds a Wi-Fi multicast lock only while the
  advertiser is active so Android does not filter discovery queries.

The application does not advertise when its HTTP listener could not bind. IPv4 is the compatibility baseline
for the Qt UDP backend; Apple Bonjour may additionally publish addresses supported by the operating system.

## Verification and troubleshooting

On macOS, browse and resolve the service with:

```sh
dns-sd -B _prg32._tcp local.
dns-sd -L 'PRG32-QT on <host>' _prg32._tcp local.
```

Then verify the resolved endpoint:

```sh
curl http://<resolved-host>:<resolved-port>/api/runtime
```

If the service is absent, confirm that PRG32-QT is running, Setup shows a valid local address, both devices are
on the same multicast-capable network, and no firewall or access-point isolation blocks UDP 5353. If discovery
works but the API does not, verify TCP port 8080 access and use the SRV port rather than assuming it. A numeric
IP URL remains a valid fallback when mDNS is unavailable.

The API is an unauthenticated local-development interface and accepts cartridge uploads. Run PRG32-QT only on
trusted networks, and do not expose port 8080 directly to the public Internet.
