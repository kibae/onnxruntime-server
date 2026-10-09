# PR and release writing

PR title: `ci: release X.Y.Z`. Use files for Markdown and reread before publishing;
preserve newlines and avoid interpolated shell heredocs for long publication text.

Cover upstream alignment, seven-file version update, each API finding and implemented/
skipped decision, revalidated assumptions, and CUDA base changes with compatibility
evidence. Include checkboxes for local Debug/CTest, exact-HEAD cross-platform CI,
local Docker tests, image publication and manual Docker Hub description update.
Check boxes only after completion.

Read the last two releases in full and retain their house style:

1. Opening: `Aligns with upstream [ONNX Runtime vX.Y.Z](https://github.com/microsoft/onnxruntime/releases/tag/vX.Y.Z).`
2. A framing paragraph; say when upstream API deltas are empty rather than padding
   with upstream's changelog.
3. `## 🎯` for a headline that warrants a section.
4. `## 🔧 Chores`: relevant commits, bold subjects and concrete effects.
5. `## 📋 Upstream X.Y.Z review`: data types, enums, setters, keys, ONNX/opset,
   version claims and assets. Link named upstream PRs.
6. `## 🐳 Docker Images`: tags/architectures, CUDA/cuDNN, driver evidence and measured
   CUDA EP results. Keep the existing manual arm64 verification practice; this skill
   does not add an arm64 execution gate.
7. Standing notes that still apply, including telemetry since 1.29.0.
8. `**Full Changelog**: https://github.com/kibae/onnxruntime-server/compare/vFROM...vTO`.

Report actual evidence with source/scope. Prior release assertions are not new
measurements. Cross-check external numbers with primary sources. Report the remaining
Docker Hub description update in the handoff.
