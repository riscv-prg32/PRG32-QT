# Debugging cartridges

PRG32-QT includes an optional desktop RV32IMAC debugger. It uses the same portable runtime as normal cartridge
execution and does not patch cartridge code or guest memory.

## Enable debug mode

1. Open **Settings** from Setup.
2. Enable **Run cartridges in RISC-V debug mode**. The setting is off by default and persists on the computer.
3. Import or select a cartridge and choose **Run Cartridge**.

The virtual PRG32 remains interactive on the left while the debugger appears on the right. Press `Escape` at
any time to pause execution and return to Setup. Desktop fullscreen intentionally hides the debugger and shows
only the aspect-correct game surface.

## Execution controls

- `⏸` pauses cartridge execution.
- `▶` resumes execution.
- `▶│` executes exactly the highlighted instruction and remains paused.
- The speed selector changes instrumented playback from `0.01×` through `4×`. It does not change the selected
  speed when Pause, Resume, or Step is used.
- The bug button selects the execution path. When highlighted, instruction-level debugging, live PC tracking,
  stepping, and breakpoints are active. When depressed, PRG32-QT uses normal whole-frame execution while keeping
  the debugger and telemetry visible.

When instruction debugging is active, the assembly window follows the live program counter. The blue row and
arrow identify the next guest instruction to execute. Register values and the selected memory range update from
the same sampled runtime state.

## Navigate assembly

The **Init**, **Update**, **Draw**, and **PC** buttons are mutually exclusive views:

- **Init** shows the cartridge initialization entry point from the PRG32 header.
- **Update** shows the per-frame update entry point.
- **Draw** shows the per-frame rendering entry point.
- **PC** follows live execution and is selected automatically by stepping or a breakpoint hit.

Changing the view does not execute code or alter registers.

## Breakpoints

Click the gutter immediately left of an instruction address to add a breakpoint. A filled red circle identifies
an active breakpoint. Click that circle again to remove only that breakpoint; any other breakpoints remain set.

PRG32-QT checks breakpoints before instruction retirement and pauses with the marked address highlighted. Resume
ignores the current breakpoint once so execution can move forward. The next visit to that address stops again.
Breakpoints apply only while the bug button is active and are cleared when another cartridge is loaded.

## Registers, memory, input, audio, and performance

The **Registers** frame shows all 32 RV32 integer registers in hexadecimal. `zero` remains hard-wired to zero.

To inspect memory, enter a decimal or `0x`-prefixed guest address and press **Inspect** or Return. PRG32-QT reads a
bounded 128-byte range and displays its hexadecimal bytes and printable ASCII representation. Invalid and
out-of-range addresses are rejected.

The telemetry frame below the virtual console provides:

- the merged digital-input bitmask from UI, keyboard, and controllers;
- the latest 64 samples rendered on the left and right mixer channels;
- measured host execution time for the last execution slice or frame;
- observed FPS, retired guest instructions, virtual cycles, and accurate-mode late frames.

The waveform display observes mixer output and does not change audio playback.

## HTTP and Python automation

The debugger is also available through `GET` and `POST /api/debug`. PRG32 SDK commands and Python clients can
use the discovered `_prg32._tcp.local.` endpoint or an explicit device URL. Typical direct requests are:

```sh
curl -X POST http://prg32-host.local:8080/api/debug \
  -H 'Content-Type: application/json' -d '{"command":"enable","enabled":true}'
curl -X POST http://prg32-host.local:8080/api/debug \
  -H 'Content-Type: application/json' -d '{"command":"pause"}'
curl -X POST http://prg32-host.local:8080/api/debug \
  -H 'Content-Type: application/json' -d '{"command":"step"}'
curl -X POST http://prg32-host.local:8080/api/debug \
  -H 'Content-Type: application/json' -d '{"command":"breakpoint","address":"0x40800100","enabled":true}'
curl -X POST http://prg32-host.local:8080/api/debug \
  -H 'Content-Type: application/json' -d '{"command":"speed","speed":0.1}'
curl 'http://prg32-host.local:8080/api/debug?address=0x40800000&length=128'
curl -X POST http://prg32-host.local:8080/api/debug \
  -H 'Content-Type: application/json' -d '{"command":"resume"}'
```

Debugger state includes the effective guest PC, update/draw phase, pause state, speed, all registers, active
breakpoints, disassembly HTML, and the requested memory bytes. Reads are limited to 1024 bytes. See
[Network discovery and SDK access](NETWORKING.md) for endpoint discovery and the complete HTTP contract.

## Troubleshooting

- If the debugger is not visible, leave fullscreen and confirm the Settings option is enabled.
- If Step is unavailable, highlight the bug button and ensure a cartridge is running.
- If a breakpoint does not stop execution, confirm the bug button is highlighted and that the address belongs
  to a decoded guest instruction.
- If controls appear unresponsive, resume execution; game input is intentionally frozen while paused.
- At very slow rates, a frame can take several seconds by design. The selected effective frame period is shown
  beside the speed selector.
