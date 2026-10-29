# Optional Play! test integration

These files preserve the project's emulator work without distributing an emulator build, BIOS, VM data, network credentials or captured private logs.

1. Clone Play! and initialize its submodules at the revisions in `UPSTREAM.json`.
2. Apply `play.patch` in the Play! root, `framework.patch` in `deps/Framework`, and `dependencies.patch` in `deps/Dependencies`.
3. Copy the `new-files/` tree over the Play! source tree.
4. Configure `tools/Rtl8192CuNetTest` with CMake and build `play-rtl8192cu-nettest`. The current harness is a Windows build and its CMake file expects MSYS2 UCRT64 libusb in `C:/msys64/ucrt64`; adapt that path to your environment.
5. Set `PS2_TEST_EMULATOR` to the built executable, provide its ordinary runtime DLL dependencies, and run `node tools/test-native-headless.cjs` from the Wi-Fi project.

The extra device class implements a synthetic RTL8192CU USB model for controlled tests. The separate physical backend is for optional Windows USB passthrough tests; it does not become part of the production PS2 ELF. Synthetic test results and Ethernet-backed emulator models must not be presented as real RF validation.

Play! and dependency licenses remain applicable; `Play-License.txt` is retained. Upstream files referenced by patches retain their notices. No Sony BIOS is included.
