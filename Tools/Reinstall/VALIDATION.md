# Offline validation record — 2026-10-10

[简体中文](VALIDATION.zh-CN.md)

2026-10-11 update: 15 preparation/upgrade tests pass; root hashes and metadata match live 25G241; actual OLED 4.7.1 routes/stub are verified and R4 runs. Main stages only 25G241 and refuses an old-build request before downloads. Clean reinstall and new-build sleep remain pending. See [current guide](../../docs/MAIN-26.7.1.md). The following is the historical pre-upgrade audit, not the current deployment state.

- Twelve Python tests pass: build allowlist, fixed URLs/hashes, archive/cache refusal, exact KDK input gates, read-only patch plans, paired Wi-Fi Add/Block ranges, unchanged rollback and security-policy refusal.
- Preparation downloaded all four omitted EFI inputs from pinned URLs, verified SHA256, and created a private staging tree without reading an old EFI as input. A second run with --offline successfully used the cache.
- Normal 25G241 reinstall preparation still refuses deployment. The separate research-only generator successfully staged four private EFIs from the owner's backed-up complete EFI: exact rollback, native installer, OLED-only test and Wi-Fi test. All four pass ocvalidate 1.0.7; the manifest still says deploymentAllowed=false and bootQualified=false.
- Staged config passes plutil and ocvalidate 1.0.7. It still has public SMBIOS placeholders, so syntax success does not mean boot-qualified.
- Current 25C56 root metadata and all four listed executable hashes match the pinned profile. Full bundle integrity and clean-install execution are not inferred.
- Multi-version OLED 4.7.0 compiles. Sanitized host tests cover malformed fingerprints, exact patterns, duplicates, per-byte mutation, truncation, call targets and relative-branch bounds. Final static audits pass all 10 patterns, seven call targets and required symbols on both 25C56 and 25G241. The 25G241 shipping SystemKC CFL UUID and all 601,466 text bytes match the KDK after external-relocation normalization. Seven object-field mappings have disassembly evidence, not runtime qualification.
- 25G241 requires both its exact driver fingerprint and an explicit isolated-test boot flag; unknown profiles refuse activation. Nine routed wrappers defer OLED-specific behavior until the patch is fully armed. Runtime routing is not atomic rollback and remains untested.
- Exact 25G241 KDK/build, CFL hash and seven root-patch executable/compatibility-library input hashes pass the read-only input audit. Static kextlibs reports duplicate _ceil/_pow exports between Libm and DspFuncLib (exit 2); actual kmutil construction and boot are still required. This is not proof that live cache construction will fail.
- A separate target-only research R4 installer was generated without altering the stable service. Syntax passes; invocation without --isolated-test refuses with exit 65 before installation. It has not been installed or tested on 25G241.
- The stable EFI, original V61 source/binary and R4 files are unchanged. No ESP write, root patch, TCC write, system install, sleep or restart was performed.

Still pending: a frozen, verified exact-source patcher build and fresh hardware detection; isolated clean-install root-patch execution/recovery; application permission reproduction; the full functional acceptance matrix; live 25G241 OLED/object-layout/R4 qualification. A same-disk test volume is not an independent backup and requires separate approval. These local changes have not been pushed as a GitHub release. See the [post-upgrade checklist](../../docs/UPGRADE-26.7.1.md).
