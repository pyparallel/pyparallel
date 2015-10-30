# WinIoCtl.h
from types cimport *

cdef extern from *:

    DWORD IOCTL_DISK_ARE_VOLUMES_READY
    DWORD IOCTL_DISK_CREATE_DISK
    DWORD IOCTL_DISK_DELETE_DRIVE_LAYOUT
    DWORD IOCTL_DISK_FORMAT_TRACKS
    DWORD IOCTL_DISK_FORMAT_TRACKS_EX
    DWORD IOCTL_DISK_GET_CACHE_INFORMATION
    DWORD IOCTL_DISK_GET_CLUSTER_INFO
    DWORD IOCTL_DISK_GET_DISK_ATTRIBUTES
    DWORD IOCTL_DISK_GET_DRIVE_GEOMETRY
    DWORD IOCTL_DISK_GET_DRIVE_GEOMETRY_EX
    DWORD IOCTL_DISK_GET_DRIVE_LAYOUT
    DWORD IOCTL_DISK_GET_DRIVE_LAYOUT_EX
    DWORD IOCTL_DISK_GET_LENGTH_INFO
    DWORD IOCTL_DISK_GET_PARTITION_INFO
    DWORD IOCTL_DISK_GET_PARTITION_INFO_EX
    DWORD IOCTL_DISK_GROW_PARTITION
    DWORD IOCTL_DISK_IS_WRITABLE
    DWORD IOCTL_DISK_PERFORMANCE
    DWORD IOCTL_DISK_PERFORMANCE_OFF
    DWORD IOCTL_DISK_REASSIGN_BLOCKS
    DWORD IOCTL_DISK_REASSIGN_BLOCKS_EX
    DWORD IOCTL_DISK_RESET_SNAPSHOT_INFO
    DWORD IOCTL_DISK_SET_CACHE_INFORMATION
    DWORD IOCTL_DISK_SET_CLUSTER_INFO
    DWORD IOCTL_DISK_SET_DISK_ATTRIBUTES
    DWORD IOCTL_DISK_SET_DRIVE_LAYOUT
    DWORD IOCTL_DISK_SET_DRIVE_LAYOUT_EX
    DWORD IOCTL_DISK_SET_PARTITION_INFO
    DWORD IOCTL_DISK_SET_PARTITION_INFO_EX
    DWORD IOCTL_DISK_UPDATE_PROPERTIES
    DWORD IOCTL_DISK_VERIFY
    DWORD IOCTL_STORAGE_QUERY_PROPERTY

    BOOL DeviceIoControl(
        (HANDLE) hDevice,                # handle to file
        FSCTL_QUERY_ALLOCATED_RANGES,    # dwIoControlCode
        (LPVOID) lpInBuffer,             # input buffer
        (DWORD) nInBufferSize,           # size of input buffer
        (LPVOID) lpOutBuffer,            # output buffer
        (DWORD) nOutBufferSize,          # size of output buffer
        (LPDWORD) lpBytesReturned,       # number of bytes returned
        (LPOVERLAPPED) lpOverlapped      # OVERLAPPED structure
    )
    ctypedef BOOL (*LPFN_DeviceIoControl)(
        (HANDLE) hDevice,                # handle to file
        FSCTL_QUERY_ALLOCATED_RANGES,    # dwIoControlCode
        (LPVOID) lpInBuffer,             # input buffer
        (DWORD) nInBufferSize,           # size of input buffer
        (LPVOID) lpOutBuffer,            # output buffer
        (DWORD) nOutBufferSize,          # size of output buffer
        (LPDWORD) lpBytesReturned,       # number of bytes returned
        (LPOVERLAPPED) lpOverlapped      # OVERLAPPED structure
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
