# windows-storage-health 1.0.0

> Runtime prerequisite for the exact qualified EXE: Microsoft Visual C++ x64 Redistributable is required when the imported runtime DLLs are absent. This supersedes any static-CRT/no-redistributable description of a source build below.


> Release context: see [qualified platforms, runtime requirements and package scope](RELEASE-QUALIFICATION.md).


A native, Unicode, read-only command-line audit of local Windows storage. Supports Windows 10, Windows 11, and Windows Server 2016–2025 with the corresponding Windows SDK APIs. Primary host validation is Windows Server 2025 x64; other generations require deployment testing. Build with CMake 3.24+, MSVC, and a Windows 10/11 SDK. No scripting engine or external program is used by the application.

**VPS limitation:** a virtual machine normally exposes the virtual disk presented by its controller, not the host's physical media. Unsupported health telemetry is reported as unavailable, never converted into a healthy verdict. A QEMU disk may identify its bus as SAS; bus type alone does not establish whether hardware is physical or virtual. This tool cannot diagnose the VPS host's disks.

```text
windows-storage-health
windows-storage-health --all
windows-storage-health --disk 0
windows-storage-health --json
windows-storage-health --all --disk 0 --json
windows-storage-health --help
windows-storage-health --version
```

Default output provides disks, Windows health evidence, volumes, capacity, findings and limitations. `--all` adds device identifiers, media context, partitions, mount points and raw evidence. JSON always contains all available detail. Disk filtering selects volumes whose extents include the requested disk; extents on other disks remain in the selected volume's relationships. Shared/spanned volumes are not truncated. Uncorrelated volumes cannot be attributed to a requested disk. Arguments use unsigned decimal disk numbers. Health findings do not change the success exit code.

| Exit | Meaning |
| --- | --- |
| 0 | Audit completed, possibly with unavailable data or findings |
| 2 | Usage error |
| 3 | Requested disk not found in enumerated devices |
| 4 | Required disk/volume enumeration API failed |
| 5 | Internal or output failure |

## Storage concepts and evidence

A storage device has a Windows device-instance identifier and may represent physical hardware, a virtual disk, RAID logical unit or Storage Spaces disk. Its disk number is a current-session identifier, not a permanent identity. A partition is a range in a disk's MBR/GPT layout. A volume is a filesystem-addressable object with its own volume GUID; it may span disks or have no mount point. Mount points expose volumes through drive letters or directories. Filesystem names and labels belong to volumes, not physical devices.

Volume-to-disk relationships come from Windows disk extents. A partition match requires identical disk number, byte offset and byte length; unmatched relationships retain `partition_number: null`. GPT partition IDs are retained; MBR identity is a signature and starting offset. No assumption equates a partition, volume and disk. Disk/device identifiers may change after hardware or VM configuration changes. Serial numbers can be empty, generic, or duplicated.

The documented `IOCTL_STORAGE_PREDICT_FAILURE` provides Windows failure prediction. Nonzero `PredictFailure` is unhealthy evidence; zero is a healthy **reported status from this particular interface**, meaning no failure prediction at that moment. It is not a comprehensive hardware diagnosis. The exact returned flag and 512 vendor bytes are preserved; vendor bytes are not decoded or obtained through vendor passthrough. Errors preserve the Win32 error number and an unavailable state. The tool has no drive-life score or invented remaining-life prediction. The model supports warning, unhealthy, healthy, unknown and unavailable evidence.

TRIM enabled is capability/context from the Windows storage stack. No TRIM operation is submitted. Seek penalty is reported as context, not used to assert SSD/HDD identity. Generic/unknown bus values remain visible. Filesystem dirty state is queried without repair; unavailable/denied state is null. A dirty flag indicates filesystem state, not failing physical media.

Capacity is in bytes in JSON and GiB in human output. Free space and caller-available space are distinct (quotas can affect the latter). Windows' returned total may also be affected by quotas. Less than 10% free produces a documented informational capacity warning, never a physical health score. Mountless boot/system volumes are included; optical and network drives are skipped.

## Safety, network and privileges

All storage operations are queries. Device and extent-query handles request access `0`, with a `GENERIC_READ` fallback for disk length on systems that require it. Filesystem dirty-state queries use a separate `GENERIC_READ` volume handle; a query-only device handle may not reach the mounted filesystem. No write access, storage mutation, raw writes, repair, reset, passthrough, benchmark, manual TRIM, policy change, mounting or shell execution exists. See [READ_ONLY_AUDIT.md](READ_ONLY_AUDIT.md) for the complete control-code/access inventory. Handle sharing flags permit coexistence with Windows; `FILE_SHARE_WRITE` does not grant write access to the tool.

There is no network client, telemetry or update checking. The tool enumerates local volume GUIDs and disk interfaces, skips network drives, and never walks filesystem contents, mount-point directories or UNC paths. Local device drivers may internally access backing infrastructure beyond the guest's control; the tool cannot determine host implementation.

Start as a standard user. Query availability depends on device, driver and ACL. Elevation can expose additional read-only evidence, but is not automatically requested. Denied queries stay visible and do not imply failure or health. Enumeration failures return 4; individual query failures allow a partial report with warnings. A changing disk/volume topology can cause incomplete results; rerun after changes settle. Device calls are synchronous and have driver-dependent latency; there is no guaranteed timeout for a stalled device driver. Buffers are bounded (up to 1 MiB for variable storage responses); unusually large responses are unavailable rather than unbounded allocations.

Event-log collection is intentionally absent in v1. `event_evidence` is an empty array. Historical disk/filesystem errors would not establish present hardware health, and lack of collected events is not evidence of an error-free history. Storage Spaces and RAID member health, vendor SMART, repair, formatting, partition management, firmware and remote storage are out of scope.

## JSON

`--json` emits UTF-8 JSON to stdout, with `schema_version: "1.0"` and `application_version: "1.0.0"`. Diagnostics for fatal errors go to stderr. Console output uses Unicode Windows output; redirected output is UTF-8 without a BOM. Optional scalar properties use null for unavailable data. Empty identifier/label strings mean no returned string. Arrays can be empty. `physical_disks`, `partitions`, `volumes`, `relationships`, `health_evidence`, `event_evidence`, `findings`, `warnings`, and `limitations` are stable top-level collections. Numeric capacities/offsets are unsigned byte counts; consumers such as JavaScript must account for integers beyond their exact numeric range. Error numbers are native Win32 values, not HRESULTs. Unknown future fields should be ignored. Reports include potentially sensitive device serials, labels, and mount paths; storage/report redaction is the caller's responsibility.

## Build, test and install

Run in an x64 MSVC developer command prompt:

```bat
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DWSH_ANALYZE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-release
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix install
```

The installed executable is `install/bin/windows-storage-health.exe`. MSVC dynamic runtime builds require the matching Microsoft Visual C++ x64 runtime. For ASan use `RelWithDebInfo` and `-DWSH_ASAN=ON`; make MSVC's `clang_rt.asan_dynamic-x86_64.dll` available through PATH or beside the executables. ASan is for validation, not distribution. `/W4` is always enabled for MSVC. `.clang-tidy` enables Clang analyzer, bug-prone and performance checks. [VALIDATION.md](VALIDATION.md) records observed tests and limitations. Tests are synthetic and read-only integration runs; they never write a storage device or generate disk fault conditions.

MIT licensed. See [LICENSE](LICENSE).
