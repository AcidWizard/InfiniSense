# Firmware Stepstodo

Open maintainability/backlog items for `Firmware/`. Referenced from
[`AGENTS.md`](AGENTS.md). Severity is in bold; references are `path:line`.

## Reliability & correctness

1. **(medium) RNG seed is not hardened.** `device::RandomSeed()` starts `NRF_RNG` but never sets `CONFIG` (bias correction) and does not ensure the HFCLK required by the RNG is running; on a fault the guard returns a partial or zero seed. Configure the RNG and document the clock requirement. (`lib/Device/src/device.cpp`)
2. **(medium) No field recovery for retained state.** `power::Reset()` clears the retained tag but is only used by tests, so a stale or corrupt retained value cannot be cleared without a reflash. Define a boot-time reset gesture (e.g. input held at power-up) or document the escape hatch. (`lib/Power/src/power.cpp`)
3. **(low) Event drops are invisible.** `mesh::Enqueue()` overwrites the oldest entry when the queue is full with no counter. Add a dropped-event count for diagnostics. (`lib/Mesh/src/mesh.cpp`)

## Functional gaps

4. **(high) Received events have no consumer.** `mesh::Handle()` enqueues events and `main::DrainEvents()` only logs them; the node never acts on remote data, so the firmware is a relay with no telemetry endpoint. Define the application delivery (consumer) that uses event payloads. (`src/main.cpp`)
5. **(medium) No reliability mechanism.** A sensor transmits each event once with no acknowledgement; loss is silent. Document the best-effort model, and consider a lightweight ack or repeat for critical events.

## Testing

6. **(medium) `radio` has no host tests.** `Send`/`Recv` length checks and failure logging are uncovered. Add a RadioLib stub and `test_radio`, or record the deliberate gap.
7. **(medium) `sleep` has no host tests.** The RTC2/GPIOTE/WFI logic is nRF-specific and cannot run on the host. Add register/interrupt stubs to exercise `Begin()` configuration and the `UntilEvent()` wake flags, or record the deliberate gap. (`lib/Sleep/src/sleep.cpp`)
8. **(low) Missing max-length boundary test.** Nothing asserts that a payload of exactly `mesh::kMaxPayload` is accepted (only that oversize is rejected). (`test/test_mesh`)
9. **(low) Role flow is untested.** `setup()`/`loop()` have no host integration test; add one if a seam is introduced. (`src/main.cpp`)

## Ops & onboarding

10. **(medium) No field flashing/DFU workflow.** Only `-t upload` over SWD is documented; add a bootloader/DFU (or OTA) path for deployed nodes.
11. **(medium) No energy budget documented.** For a battery-free device, document per-event energy, System ON idle current (RTC/LFCLK), heartbeat duty cycle and the resulting average against the harvester, so payload/rate changes can be judged. **Needs hardware.**
12. **(low) No license headers.** Source files carry no SPDX/copyright while the repo is MIT. Add SPDX headers or document the omission.
13. **(low) Duplicated quality commands.** The format and `cppcheck` invocations are repeated between `CONTRIBUTING.md` and CI; consider a single script or config file.

## Protocol design

14. **(medium) Dedup window is fixed at 16 with no time component.** Review sizing and broadcast-storm risk as node count or event rate grows. (`config::kMeshDedupSize`)
