# XFrames Project Documentation

The live [xframes.dev](https://xframes.dev) website is maintained in a separate repository. This directory also contains legacy Jekyll and generated demo assets; the documents indexed below are repository-level engineering and strategy records.

## Strategy

- [XFrames and GPUIX: Technical and Strategic Assessment](strategy/gpuix-comparison-2026-08.md) — dated comparison, architectural corrections, dependency and adoption risks, strategic focus, and continuation gates.

## Architecture

- [React Native Fabric Embedding](../packages/dear-imgui/npm/FABRIC_EMBEDDING.md) — implemented React Native 0.87 snapshot generation, host contract, workspace rules, upgrade procedure, Node/Wasm verification, and known gaps.
- [Fabric-Compatible Runtime Hardening](architecture/fabric-runtime-hardening.md) — atomic Fabric commits, explicit destruction, lifecycle tests, recording/replay, invalidation-driven rendering, performance instrumentation, and automation.

## Engineering and verification

- [ubx-monitor Ordinary Application Qualification](engineering/ubx-monitor-application-2026-09.md) — completed bounded Windows slice: current-package full-App integration, sustained synthetic telemetry across Plot/Table/Map/Canvas, connection lifecycle, cleanup and reproducible setup.
- [Stage 4: Invalidation Scheduling](engineering/fabric-invalidation-2026-09.md) — delivered scheduler MVP, producer and lifetime boundaries, local validation, measurements, completed hosted CI stabilization and deferred qualification.
- [Stage 3: Atomic Fabric Publication](engineering/fabric-publication-2026-09.md) — schema-v2 publication, committed ownership, visibility guarantees and historical acceptance evidence.
- [Diagnostics and Reproduction Guide](../packages/dear-imgui/npm/diagnostics/README.md) — current fixture commands, platform prerequisites and measurement contracts.

## Existing design records

- [Canvas Widget Design](../CANVAS.md) — canvas purpose, data flow, draw commands, performance model, and integration patterns.
- [Project Roadmap](../ROADMAP.md) — current delivery phases and runtime-hardening milestones.

## Documentation conventions

- Strategy assessments are dated snapshots. Update them with a new dated document when the competitive or dependency landscape materially changes.
- Architecture documents describe proposed or accepted runtime behavior. Keep status and update dates at the top.
- The roadmap tracks delivery status and links to detailed documents instead of duplicating their design content.
- Public product documentation belongs in the separate xframes.dev source repository.
