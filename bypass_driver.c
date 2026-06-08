#include <ntddk.h>
#include <windef.h>
#include <intrin.h>

#define BYPASS_DEVICE_NAME L"\\Device\\SatellaBypass"
#define BYPASS_DOS_NAME L"\\DosDevices\\SatellaBypass"
#define IOCTL_BYPASS_PROTECT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BYPASS_HIDE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BYPASS_PATCH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BYPASS_CLEAN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define PROCESS_TERMINATE 0x0001
#define THREAD_CREATE 0x0002

static PDEVICE_OBJECT g_devObj = NULL;
static PDRIVER_DISPATCH g_OriginalDispatch = NULL;
static UNICODE_STRING g_targetDev;
static LIST_ENTRY g_hookedPids;
static KSPIN_LOCK g_hookLock;
static BOOLEAN g_hookInstalled = FALSE;

typedef struct _BYTE_PATCH {
    LIST_ENTRY entry;
    PVOID address;
    UCHAR original[16];
    UCHAR patch[16];
    USHORT size;
} BYTE_PATCH, *PBYTE_PATCH;

typedef struct _BYPASS_REQUEST {
    ULONG code;
    UCHAR data[256];
} BYPASS_REQUEST, *PBYPASS_REQUEST;

typedef struct _PROTECTED_PID {
    LIST_ENTRY entry;
    ULONG pid;
} PROTECTED_PID, *PPROTECTED_PID;

static VOID ProtectProcess(ULONG pid) {
    PPROTECTED_PID pp = (PPROTECTED_PID)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(PROTECTED_PID), 'tPBs');
    if (!pp) return;
    pp->pid = pid;
    KIRQL irql; KeAcquireSpinLock(&g_hookLock, &irql);
    InsertTailList(&g_hookedPids, &pp->entry);
    KeReleaseSpinLock(&g_hookLock, irql);
    DbgPrint("SatellaBypass: Protecting PID %lu\n", pid);
}

static BOOLEAN IsProtected(ULONG pid) {
    KIRQL irql; KeAcquireSpinLock(&g_hookLock, &irql);
    PLIST_ENTRY it = g_hookedPids.Flink;
    while (it != &g_hookedPids) {
        PPROTECTED_PID pp = CONTAINING_RECORD(it, PROTECTED_PID, entry);
        if (pp->pid == pid) { KeReleaseSpinLock(&g_hookLock, irql); return TRUE; }
        it = it->Flink;
    }
    KeReleaseSpinLock(&g_hookLock, irql);
    return FALSE;
}

NTSTATUS MyNtOpenProcess(ULONG pid, HANDLE* hProc) {
    if (IsProtected(pid)) return STATUS_ACCESS_DENIED;
    return STATUS_SUCCESS;
}

NTSTATUS MyNtOpenThread(HANDLE hProc, ULONG tid) {
    return STATUS_SUCCESS;
}

static NTSTATUS PatchMemory(PVOID addr, PVOID patchData, USHORT size) {
    BYTE_PATCH* bp = (BYTE_PATCH*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(BYTE_PATCH), 'hPsB');
    if (!bp) return STATUS_INSUFFICIENT_RESOURCES;
    bp->address = addr;
    bp->size = size;
    RtlCopyMemory(bp->original, addr, size);
    RtlCopyMemory(bp->patch, patchData, size);

    KIRQL irql = KeRaiseIrqlToDpcLevel();
    RtlCopyMemory(addr, patchData, size);
    KeLowerIrql(irql);
    InsertTailList(&g_hookedPids, &bp->entry);
    DbgPrint("SatellaBypass: Patched 0x%p (%hu bytes)\n", addr, size);
    return STATUS_SUCCESS;
}

static VOID RestoreAllPatches() {
    PLIST_ENTRY it = g_hookedPids.Flink;
    while (it != &g_hookedPids) {
        BYTE_PATCH* bp = CONTAINING_RECORD(it, BYTE_PATCH, entry);
        KIRQL irql = KeRaiseIrqlToDpcLevel();
        RtlCopyMemory(bp->address, bp->original, bp->size);
        KeLowerIrql(irql);
        it = it->Flink;
    }
}

static NTSTATUS OnDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG info = 0;

    if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        ULONG ctl = stack->Parameters.DeviceIoControl.IoControlCode;
        PBYPASS_REQUEST req = (PBYPASS_REQUEST)Irp->AssociatedIrp.SystemBuffer;

        switch (ctl) {
        case IOCTL_BYPASS_PROTECT:
            if (req) {
                ULONG pid = *(ULONG*)req->data;
                ProtectProcess(pid);
                info = 1;
            }
            break;
        case IOCTL_BYPASS_HIDE:
            if (req) {
                ULONG pid = *(ULONG*)req->data;
                ProtectProcess(pid);
                DbgPrint("SatellaBypass: Hiding PID %lu\n", pid);
                info = 1;
            }
            break;
        case IOCTL_BYPASS_PATCH:
            if (req) {
                PVOID addr = *(PVOID*)req->data;
                UCHAR nop[16] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
                PatchMemory(addr, nop, 8);
                info = 1;
            }
            break;
        case IOCTL_BYPASS_CLEAN:
            RestoreAllPatches();
            info = 1;
            break;
        }
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = info;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
    if (g_OriginalDispatch && g_hookInstalled)
        return ((PDRIVER_DISPATCH)g_OriginalDispatch)(DeviceObject, Irp);
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static VOID OnUnload(PDRIVER_OBJECT DriverObject) {
    RestoreAllPatches();
    UNICODE_STRING dos;
    RtlInitUnicodeString(&dos, BYPASS_DOS_NAME);
    IoDeleteSymbolicLink(&dos);
    if (g_devObj) IoDeleteDevice(g_devObj);
    if (g_OriginalDispatch && g_hookInstalled) {
        PFILE_OBJECT fObj = NULL;
        PDEVICE_OBJECT dObj = NULL;
        if (NT_SUCCESS(IoGetDeviceObjectPointer(&g_targetDev, FILE_READ_DATA, &fObj, &dObj))) {
            if (dObj && dObj->DriverObject) {
                InterlockedExchangePointer(
                    (PVOID*)&dObj->DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL],
                    (PVOID)g_OriginalDispatch
                );
            }
            if (fObj) ObDereferenceObject(fObj);
        }
    }
    PLIST_ENTRY it = g_hookedPids.Flink;
    while (it != &g_hookedPids) {
        PPROTECTED_PID pp = CONTAINING_RECORD(it, PROTECTED_PID, entry);
        it = it->Flink;
        RemoveEntryList(&pp->entry);
        ExFreePool(pp);
    }
    DbgPrint("SatellaBypass: Unloaded\n");
}

NTSTATUS InstallHook(PCWSTR targetDeviceName) {
    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, targetDeviceName);
    RtlInitUnicodeString(&g_targetDev, targetDeviceName);
    PFILE_OBJECT fileObj = NULL;
    PDEVICE_OBJECT devObj = NULL;
    NTSTATUS status = IoGetDeviceObjectPointer(&devName, FILE_READ_DATA, &fileObj, &devObj);
    if (!NT_SUCCESS(status)) return status;
    PDRIVER_OBJECT targetDriver = devObj->DriverObject;
    g_OriginalDispatch = (PDRIVER_DISPATCH)InterlockedExchangePointer(
        (PVOID*)&targetDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL],
        (PVOID)OnDispatch
    );
    ObDereferenceObject(fileObj);
    g_hookInstalled = TRUE;
    DbgPrint("SatellaBypass: Hook installed on %ws\n", targetDeviceName);
    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNICODE_STRING devName, dosName;
    RtlInitUnicodeString(&devName, BYPASS_DEVICE_NAME);
    RtlInitUnicodeString(&dosName, BYPASS_DOS_NAME);
    InitializeListHead(&g_hookedPids);
    KeInitializeSpinLock(&g_hookLock);

    NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_devObj);
    if (!NT_SUCCESS(status)) return status;

    status = IoCreateSymbolicLink(&dosName, &devName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_devObj);
        return status;
    }

    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = OnDispatch;
    DriverObject->DriverUnload = OnUnload;

    InstallHook(L"\\Device\\Satella");

    DbgPrint("SatellaBypass: Driver loaded successfully\n");
    return STATUS_SUCCESS;
}
