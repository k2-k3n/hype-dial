# Resona developer guides

Copied from the Resona SDK into every project (read-only; `resona-lab sdk sync .` refreshes
them together with the project's SDK pin; legacy local copies remain supported).

| File | For |
| --- | --- |
| [resona-box.md](resona-box.md) | the device: audio, block sizes, CPU and memory limits, screen, encoders |
| [cpu-qualification.md](cpu-qualification.md) | target CPU trials, workload hook and reservation floor |
| [dsp-tools.md](dsp-tools.md) | portable/native DSP kernels and named device profiling |
| [dsp-memory.md](dsp-memory.md) | choose fast SRAM or PSRAM buffers, lifetime and admission |
| [packs.md](packs.md) | algorithm packs: the module API, real-time rules, screens, the UI kit |
| [box-apps.md](box-apps.md) | box apps: how they run, the platform API by topic |
| [ui-libraries.md](ui-libraries.md) | optional App UI libraries: Box UI widget kit and the LVGL 9 port |
| [tools.md](tools.md) | the `resona-lab` commands and the MCP server for AI agents |
| [recipes.md](recipes.md) | worked changes for packs and apps |

Custom-chain commands: `resona-lab chain custom` starts with one dry slot; `chain slots N`
adds slots (up to five), `chain status --json` reports live target CPU/RAM/code resources.
Import with `pack try PROJECT --block N`; the device measures and gates resource use before
activation. CPU values from RESONA_SIM_MIDI are simulator observations, not hardware timing.
