# Contributing

Thanks for helping. A few rules keep the project safe to host:

- **No game data, ever.** That covers archives, the executable, carved files, shaders (DXBC, FSSM, disassembly,
  SPIR-V), textures, models, audio, videos, and reports or logs that quote them. `.gitignore` blocks the common
  types; check `git status` before you commit anyway.
- **No keys, decryption or protection workarounds**, and no links to where to get them. The tools only read files
  that are already plain on disk.
- Short excerpts needed to explain a format (a few header bytes, one instruction sequence) are fine in docs and
  comments.

## Before you start

Anyone with a GitHub account can [open an issue](https://github.com/nufu88pa/retun-leonida-for-nufu/issues/new/choose) or propose a pull request. Documentation, independent reviews and tests with synthetic inputs are welcome.

1. Check the [coordination issue](https://github.com/nufu88pa/retun-leonida-for-nufu/issues?q=is%3Aissue+is%3Aopen+in%3Atitle+%22Coordinating+who+works+on+what%22) and [open pull requests](https://github.com/nufu88pa/retun-leonida-for-nufu/pulls) for overlapping work. If you plan to take a task, post `claiming: <ID>` or a one-line description in the coordination issue. Small fixes can link to the relevant issue or explain the change in the pull request.
2. [Fork this repository](https://github.com/nufu88pa/retun-leonida-for-nufu/fork), branch from `main` and commit your changes to your fork.
3. Open a pull request against this repository's `main`. Link the issue or claim, describe what changed, and list the checks you actually ran. State whether the work was AI-assisted. For GPU reports, include the tested commit, OS, GPU, driver, and results using synthetic inputs when possible.

## Working on the code

- Python tools run with `python -I` and use only the standard library unless a file says otherwise.
- A text file a tool writes (JSON, CSV, a manifest, an assembly listing) names its line ending: `open(path, "w",
  encoding="utf-8", newline="\n")` (the same for `Path.open`, `Path.write_text` and a text-mode
  `tempfile.NamedTemporaryFile`), so the output is LF on Windows too and the same input gives the same bytes on every
  platform. `tests/test_text_write_line_endings.py` fails for a writer that forgets it.
- A report writer refuses an output that names one of its inputs or another output, and asks
  `tools/report_paths.same_report_path(a, b)` rather than deriving the answer again: it resolves both paths,
  applies the volume's case rules and falls back to `os.path.samefile`, so a new tool reuses one answer
  instead of a private copy. `tests/test_report_paths.py` covers it.
- `tools/rpf9dump` needs the submodule: `git submodule update --init`.
- A new shader-translator rule goes into the contract table in `docs/design.md`, with how it was confirmed: a GPU
  test, a Mesa/LLVM reference, or a count over the corpus.
- `python -I tests/test_<name>.py` runs one of the unit tests (standard library only; CI runs them all). The test
  files with no `__main__` block or an `import pytest` (they need the packages in `tests/requirements.txt`) run with
  `python -m pytest tests/test_<name>.py` instead, as [AGENTS.md](AGENTS.md) describes; under `python -I` they fail
  to import or run nothing.
- Runtime tests: `cmake -S runtime -B runtime/build`, `cmake --build runtime/build --config RelWithDebInfo`, then
  `ctest --test-dir runtime/build -C RelWithDebInfo --output-on-failure`. The CPU tests that need no game data are configured
  on Windows; Linux and macOS configure the portable subset (`gta6rt` and the shims are Windows-only). Tests that need the retail package read `LEONIDA_GAME_DIR` (and `LEONIDA_MOVIE` for the Bink
  decode test) in place and print `SKIP` when the variable is unset; never copy or commit those files.
- Viewer streaming tests: `node --test tests/test_viewer_streaming.cjs` (Node.js 22+, CI tests 24 LTS; no packages,
  browser, network or game data). These execute the viewer streaming code with a synthetic loader and clock.
- `loader_test` writes small PE images to a `leonida_loader_test` folder in `%TEMP%`. Antivirus that blocks newly
  written executables makes it stop with that reason; exclude the folder or set `LEONIDA_TEST_DIR` to one it skips.
- Tool checks: `python -m compileall -q make_game.py tools tests`. The optional
  `vk_pipeline_check <spirv dir> [cache.bin] [--jobs N]` creates a module (and, for compute shaders, a
  pipeline) for every translated shader and reports failures plus the saved cache path.
- GPU tests are small hand-written RDNA2 programs in `runtime/test/shaders`. `build.py` turns them into SPIR-V
  using `llvm-mc` with the AMDGPU target (LLVM 21+). `check.py` runs them on your GPU and compares the results.
  A new test gets its expected values in the comment at its top and a check in `check.py`.
  [docs/testing.md](docs/testing.md) covers setup per OS, including native Windows options, and known problems.
- Say what you tested and on which GPU in the pull request.
- Run the translator tests with `python -I scripts/run_semantic_tests.py tests/semantic` (see `tests/semantic/README.md`). A new
  translator rule should come with a small hand-written `.asm` fixture and its expected output words.
