# Read-only and no-network audit

> Release context: see [qualified platforms, runtime requirements and package scope](RELEASE-QUALIFICATION.md).


Every production `DeviceIoControl` request passes through `query`, whose explicit switch accepts only the following read-only controls. No caller-supplied control code enters the application. Unknown codes are rejected locally. No vendor/pass-through request is allowed.

| Control | Purpose | Handle access |
| --- | --- | --- |
| IOCTL_STORAGE_GET_DEVICE_NUMBER | Map local disk interface to disk number | 0 |
| IOCTL_STORAGE_QUERY_PROPERTY | Device descriptor, TRIM, seek penalty; StandardQuery only | 0 |
| IOCTL_STORAGE_PREDICT_FAILURE | Windows-reported failure prediction | 0 |
| IOCTL_DISK_GET_LENGTH_INFO | Disk byte length | 0 first; GENERIC_READ fallback |
| IOCTL_DISK_GET_DRIVE_GEOMETRY_EX | Query disk size if length unavailable | 0 |
| IOCTL_DISK_GET_DRIVE_LAYOUT_EX | Read partition layout | 0 |
| IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS | Volume-to-disk ranges | 0 |
| FSCTL_IS_VOLUME_DIRTY | Read filesystem dirty state | GENERIC_READ |

`CreateFileW` is centralized in `open`, uses OPEN_EXISTING, no write rights, and sharing READ/WRITE/DELETE. Sharing is permission for other handles, not requested access. Handles and enumeration handles use RAII. There are no WriteFile, filesystem setters, registry setters, mutating IOCTLs, token elevation, raw storage writes, process creation, shell execution, CHKDSK, SMART passthrough or reset paths in production. The CLI writes its report only to stdout/stderr. CMake install and test/build artifacts write ordinary project files, not raw storage.

Dirty-state acquisition opens the enumerated local volume GUID with its trailing backslash removed, e.g. `\\?\Volume{GUID}`. This is a volume open, not a root-directory open. It uses a separate read-access handle for the filesystem control; query-only access remains sufficient for disk extents. A failed open or control, including unsupported filesystem/volume stacks, leaves dirty state null. A successful response shorter than a DWORD is explicitly unavailable, without consulting stale last-error state. Only a complete successful response is interpreted using `VOLUME_IS_DIRTY`.

SetupAPI enumerates only present GUID_DEVINTERFACE_DISK interfaces. Volume enumeration uses FindFirstVolume/FindNextVolume GUID names; no drive-letter/UNC crawling occurs. GetDriveType rejects DRIVE_REMOTE before any volume information or free-space queries. Only local fixed/removable/RAM volumes are accepted; optical/unknown drives are skipped. Mount points are obtained as metadata and printed, never opened. No Winsock, WinHTTP, WinINet, URL APIs, remote registry or network connection is linked or invoked. Device drivers and hypervisors may implement local devices over remote infrastructure; that is outside the tool's visibility.

Microsoft API references:

- [Failure prediction](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddstor/ni-ntddstor-ioctl_storage_predict_failure)
- [Storage properties](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-ioctl_storage_query_property)
- [Disk layouts](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-ioctl_disk_get_drive_layout_ex)
- [Volume extents](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-ioctl_volume_get_volume_disk_extents)
- [Filesystem dirty query contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/fsctl-is-volume-dirty)
- [CreateFile volume access and sharing](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)
- [Volume GUID syntax](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-volume)
