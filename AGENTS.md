# AGENTS.md

## Scope

This repository supports exactly one board: `folo/ai-passport-c3` on ESP32-C3.
Do not add or restore unrelated board implementations unless the repository
scope is intentionally changed.

## Rules

- Use `scripts/build.py` as the canonical build entry point.
- Keep exactly one `DECLARE_BOARD(...)` factory.
- Keep board-specific pins and initialization under
  `main/boards/folo/ai-passport-c3/`.
- Core code must depend on the common `Board` interfaces.
- Schedule application mutations from callbacks with
  `Application::Schedule()`.
- Do not manually edit generated files under `build/`, `managed_components/`,
  `components/`, generated assets, or `sdkconfig*`.
- Preserve `LICENSE` and `NOTICE` when redistributing the project.
- Format touched C/C++ files with the repository `.clang-format`.

## Validation

```sh
python -m unittest discover -s scripts/tests -v
python scripts/build.py folo/ai-passport-c3 \
  --name folo-ai-passport-c3 \
  --language zh-CN \
  --wake-word nihaoxiaozhi
```

Report build validation separately from physical screen, audio, button, Wi-Fi,
and wake-word validation.

