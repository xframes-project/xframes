# XFrames Project Documentation

The live [xframes.dev](https://xframes.dev) website is maintained in a separate repository. This directory also contains legacy Jekyll and generated demo assets; the documents indexed below are repository-level engineering and strategy records.

## Strategy

- [XFrames and GPUIX: Technical and Strategic Assessment](strategy/gpuix-comparison-2026-08.md) — dated comparison, architectural corrections, dependency and adoption risks, strategic focus, and continuation gates.

## Architecture

- [Fabric-Compatible Runtime Hardening](architecture/fabric-runtime-hardening.md) — atomic Fabric commits, explicit destruction, lifecycle tests, recording/replay, invalidation-driven rendering, performance instrumentation, and automation.

## Existing design records

- [Canvas Widget Design](../CANVAS.md) — canvas purpose, data flow, draw commands, performance model, and integration patterns.
- [Project Roadmap](../ROADMAP.md) — current delivery phases and runtime-hardening milestones.

## Documentation conventions

- Strategy assessments are dated snapshots. Update them with a new dated document when the competitive or dependency landscape materially changes.
- Architecture documents describe proposed or accepted runtime behavior. Keep status and update dates at the top.
- The roadmap tracks delivery status and links to detailed documents instead of duplicating their design content.
- Public product documentation belongs in the separate xframes.dev source repository.
