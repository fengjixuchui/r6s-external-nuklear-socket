#include "server_shared.h"
#include "sockets.h"
#include "imports.h"

static uint64_t handle_copy_memory(const PacketCopyMemory& packet)
{
	KeEnterGuardedRegion();

	PEPROCESS dest_process = nullptr;
	PEPROCESS src_process = nullptr;

	if (!NT_SUCCESS(PsLookupProcessByProcessId(HANDLE(packet.dest_process_id), &dest_process)))
	{
		return uint64_t(STATUS_INVALID_CID);
	}

	if (!NT_SUCCESS(PsLookupProcessByProcessId(HANDLE(packet.src_process_id), &src_process)))
	{
		ObDereferenceObject(dest_process);
		return uint64_t(STATUS_INVALID_CID);
	}

	SIZE_T   return_size = 0;
	NTSTATUS status = MmCopyVirtualMemory(
		src_process,
		(void*)packet.src_address,
		dest_process,
		(void*)packet.dest_address,
		packet.size,
		UserMode,
		&return_size
	);

	ObDereferenceObject(dest_process);
	ObDereferenceObject(src_process);

	KeLeaveGuardedRegion();

	return uint64_t(status);
}

static uint64_t handle_get_base_address(const PacketGetBaseAddress& packet)
{
	KeEnterGuardedRegion();

	PEPROCESS process = nullptr;
	NTSTATUS  status = PsLookupProcessByProcessId(HANDLE(packet.process_id), &process);

	if (!NT_SUCCESS(status))
		return 0;

	const auto base_address = uint64_t(PsGetProcessSectionBaseAddress(process));
	ObDereferenceObject(process);

	KeLeaveGuardedRegion();

	return base_address;
}

static uint64_t handle_clean_piddb_cache() {
	KeEnterGuardedRegion();

	log("clean_piddb_cache started!");

	size_t size;
	uintptr_t ntoskrnlBase = get_kernel_address("ntoskrnl.exe", size);

	log("ntoskrnl.exe: %d\n", ntoskrnlBase);
	log("ntoskrnl.exe size: %d\n", size);

	// 1809: \x48\x8D\x0D\x00\x00\x00\x00\x4C\x89\x35\x00\x00\x00\x00\x49 xxx????xxx????x
	// 1903: \x48\x8d\x0d\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x3d\x00\x00\x00\x00\x0f\x83 xxx????x????x????xx

	RTL_OSVERSIONINFOW osVersion = { 0 };
	osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	RtlGetVersion(&osVersion);

	log("Windows Build Number: %d\n", osVersion.dwBuildNumber);
	log("Windows Major Version: %d\n", osVersion.dwMajorVersion);
	log("Windows Minor Version: %d\n", osVersion.dwMinorVersion);

	PRTL_AVL_TABLE PiDDBCacheTable = nullptr;

	if (osVersion.dwBuildNumber >= 18362)
		PiDDBCacheTable = (PRTL_AVL_TABLE)dereference(find_pattern<uintptr_t>((void*)ntoskrnlBase, size, "\x48\x8d\x0d\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x3d\x00\x00\x00\x00\x0f\x83", "xxx????x????x????xx"), 3);
	else if (osVersion.dwBuildNumber >= 17763)
		PiDDBCacheTable = (PRTL_AVL_TABLE)dereference(find_pattern<uintptr_t>((void*)ntoskrnlBase, size, "\x48\x8D\x0D\x00\x00\x00\x00\x4C\x89\x35\x00\x00\x00\x00\x49", "xxx????xxx????x"), 3);
	else if (osVersion.dwBuildNumber >= 17134)
		PiDDBCacheTable = (PRTL_AVL_TABLE)dereference(find_pattern<uintptr_t>((void*)ntoskrnlBase, size, "\x48\x8D\x0D\x00\x00\x00\x00\x4C\x89\x35\x00\x00\x00\x00\x49", "xxx????xxx????x"), 3);

	log("PiDDBCacheTable: %d\n", PiDDBCacheTable);

	if (!PiDDBCacheTable) {
		log("PiDDBCacheTable equals 0\n");
		return 0;
	}

	uintptr_t entry_address = uintptr_t(PiDDBCacheTable->BalancedRoot.RightChild) + sizeof(RTL_BALANCED_LINKS);
	log("entry_address: %d\n", entry_address);

	piddbcache* entry = (piddbcache*)(entry_address);

	/*capcom.sys(drvmap) : 0x57CD1415 iqvw64e.sys(kdmapper) : 0x5284EAC3, also cpuz driver*/
	if (entry->TimeDateStamp == 0x57CD1415 || entry->TimeDateStamp == 0x5284EAC3) {
		entry->TimeDateStamp = 0x54EAC3;
		entry->DriverName = RTL_CONSTANT_STRING(L"monitor.sys");
	}

	ULONG count = 0;
	for (auto link = entry->List.Flink; link != entry->List.Blink; link = link->Flink, count++)
	{
		piddbcache* cache_entry = (piddbcache*)(link);

		log("cache_entry count: %lu name: %wZ \t\t stamp: %x\n",
			count,
			cache_entry->DriverName,
			cache_entry->TimeDateStamp);

		if (cache_entry->TimeDateStamp == 0x57CD1415 || cache_entry->TimeDateStamp == 0x5284EAC3) {
			cache_entry->TimeDateStamp = 0x54EAC4 + count;
			cache_entry->DriverName = RTL_CONSTANT_STRING(L"monitor.sys");
		}
	}

	log("clean_piddb_cache finished!");

	KeLeaveGuardedRegion();

	return 1;
}

static uint64_t handle_clean_unloaded_drivers() {
	KeEnterGuardedRegion();

	log("clean_uloaded_drivers started!\n");
	ULONG bytes = 0;
	auto status = ZwQuerySystemInformation(SystemModuleInformation, 0, bytes, &bytes);

	if (!bytes)
		return 0;

	PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)ExAllocatePool(NonPagedPool, bytes);

	status = ZwQuerySystemInformation(SystemModuleInformation, modules, bytes, &bytes);

	if (!NT_SUCCESS(status)) {
		log("ZwQuerySystemInformation failed(unloaded drivers)\n");
		ExFreePool(modules);
		return 0;
	}

	PRTL_PROCESS_MODULE_INFORMATION module = modules->Modules;
	uintptr_t ntoskrnlBase = 0;
	size_t ntoskrnlSize = 0;

	ntoskrnlBase = get_kernel_address("ntoskrnl.exe", ntoskrnlSize);

	ExFreePool(modules);

	if (ntoskrnlBase <= 0) {
		log("get_kernel_address failed(unloaded drivers)\n");
		return 0;
	}

	// 1809: \x48\x8B\x05\x00\x00\x00\x00\x48\xF7\xD8\x1B\xC9\x81 xxx????xxxxxx
	// 1903: \x4C\x8B\x00\x00\x00\x00\x00\x4C\x8B\xC9\x4D\x85\x00\x74 xx?????xxxxx?x + 3 + current signature address = MmUnloadedDrivers

	RTL_OSVERSIONINFOW osVersion = { 0 };
	osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	RtlGetVersion(&osVersion);

	log("Windows Build Number: %d\n", osVersion.dwBuildNumber);
	log("Windows Major Version: %d\n", osVersion.dwMajorVersion);
	log("Windows Minor Version: %d\n", osVersion.dwMinorVersion);

	uintptr_t mmUnloadedDriversPtr = 0;

	if (osVersion.dwBuildNumber >= 18362)
		mmUnloadedDriversPtr = find_pattern<uintptr_t>((void*)ntoskrnlBase, ntoskrnlSize, "\x4C\x8B\x00\x00\x00\x00\x00\x4C\x8B\xC9\x4D\x85\x00\x74", "xx?????xxxxx?x");
	else if (osVersion.dwBuildNumber >= 17763)
		mmUnloadedDriversPtr = find_pattern<uintptr_t>((void*)ntoskrnlBase, ntoskrnlSize, "\x48\x8B\x05\x00\x00\x00\x00\x48\xF7\xD8\x1B\xC9\x81", "xxx????xxxxxx");
	else if (osVersion.dwBuildNumber >= 17134)
		mmUnloadedDriversPtr = find_pattern<uintptr_t>((void*)ntoskrnlBase, ntoskrnlSize, "\x48\x8B\x05\x00\x00\x00\x00\x48\xF7\xD8\x1B\xC9\x81", "xxx????xxxxxx");

	log("mmUnloadedDriversPtr: %d\n", mmUnloadedDriversPtr);

	if (!mmUnloadedDriversPtr) {
		log("mmUnloadedDriversPtr equals 0(unloaded drivers)\n");
		return 0;
	}

	uintptr_t mmUnloadedDrivers = dereference(mmUnloadedDriversPtr, 3);

	memset(*(uintptr_t * *)mmUnloadedDrivers, 0, 0x7D0);

	log("clean_uloaded_drivers finished!\n");

	KeLeaveGuardedRegion();

	return 1;
}

uint64_t handle_hwid_spoofing() {
	KeEnterGuardedRegion();

	// 1809: \x4C\x8B\xDC\x49\x89\x5B\x10\x49\x89\x6B\x18\x49\x89\x73\x20\x57\x48\x83\xEC\x50 xxxxxxxxxxxxxxxxxxxx
	// 1903: \x48\x89\x5C\x24\x00\x55\x56\x57\x48\x83\xEC\x50\x8B xxxx?xxxxxxxx

	uintptr_t storportBase = 0;
	size_t storportSize = 0;

	storportBase = get_kernel_address("storport.sys", storportSize);

	if (storportBase <= 0) {
		log("get_kernel_address failed(storport)\n");
		return 0;
	}

	RTL_OSVERSIONINFOW osVersion = { 0 };
	osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	RtlGetVersion(&osVersion);

	log("Windows Build Number: %d\n", osVersion.dwBuildNumber);
	log("Windows Major Version: %d\n", osVersion.dwMajorVersion);
	log("Windows Minor Version: %d\n", osVersion.dwMinorVersion);

	if (osVersion.dwBuildNumber >= 18362)
		pRegDevInt = (RaidUnitRegisterInterfaces)dereference(find_pattern<uintptr_t>((void*)storportBase, storportSize, "\x48\x89\x5C\x24\x00\x55\x56\x57\x48\x83\xEC\x50\x8B", "xxxx?xxxxxxxx"), 0);
	else if (osVersion.dwBuildNumber >= 17763)
		pRegDevInt = (RaidUnitRegisterInterfaces)dereference(find_pattern<uintptr_t>((void*)storportBase, storportSize, "\x4C\x8B\xDC\x49\x89\x5B\x10\x49\x89\x6B\x18\x49\x89\x73\x20\x57\x48\x83\xEC\x50", "xxxxxxxxxxxxxxxxxxxx"), 0);

	log("pRegDevIntPtr: %d\n", pRegDevInt);

	PDEVICE_OBJECT pObject = NULL;
	PFILE_OBJECT pFileObj = NULL;

	UNICODE_STRING DestinationString;

	RtlInitUnicodeString(&DestinationString, L"\\Device\\RaidPort0");

	NTSTATUS status = IoGetDeviceObjectPointer(&DestinationString, FILE_READ_DATA, &pFileObj, &pObject);

	PDRIVER_OBJECT pDriver = pObject->DriverObject;

	PDEVICE_OBJECT pDevice = pDriver->DeviceObject;

	while (pDevice->NextDevice != NULL)
	{
		if (pDevice->DeviceType == FILE_DEVICE_DISK)
		{
			PHDD_EXTENSION pDeviceHDD = (PHDD_EXTENSION)pDevice->DeviceExtension;

			CHAR HDDSPOOFED_TMP[32] = { 0x0 };

			randstring(HDDSPOOFED_TMP, SERIAL_MAX_LENGTH - 1);

			for (int i = 1; i <= SERIAL_MAX_LENGTH + 1; i++)
			{
				log("a i: %d\n", i);
				memcpy(&HDDORG_BUFFER[HDD_count][i], &pDeviceHDD->pHDDSerial[i], sizeof(CHAR));
				log("b i: %d\n", i);
				memcpy(&HDDSPOOF_BUFFER[HDD_count][i], &HDDSPOOFED_TMP[i], sizeof(CHAR));
				log("c i: %d\n", i);
			}

			RtlStringCchPrintfA(pDeviceHDD->pHDDSerial, SERIAL_MAX_LENGTH + 1, "%s", &HDDSPOOFED_TMP);

			log("RtlStringCchPrintfA");

			pRegDevInt(pDeviceHDD);

			log("pRegDevInt: %d\n", pDeviceHDD);

			HDD_count++;
		}

		pDevice = pDevice->NextDevice;
	}

	KeLeaveGuardedRegion();
}

uint64_t handle_incoming_packet(const Packet& packet)
{
	KeEnterGuardedRegion();
	switch (packet.header.type)
	{
	case PacketType::packet_copy_memory:
		return handle_copy_memory(packet.data.copy_memory);

	case PacketType::packet_get_base_address:
		return handle_get_base_address(packet.data.get_base_address);

	case PacketType::packet_clean_piddbcachetable:
		return handle_clean_piddb_cache();

	case PacketType::packet_clean_mmunloadeddrivers:
		return handle_clean_unloaded_drivers();

	case PacketType::packet_hwid_spoofing:
		return handle_hwid_spoofing();

	default:
		break;
	}

	KeLeaveGuardedRegion();

	return uint64_t(STATUS_NOT_IMPLEMENTED);
}

// Send completion packet.
bool complete_request(const SOCKET client_connection, const uint64_t result)
{
	KeEnterGuardedRegion();

	Packet packet{ };

	packet.header.magic = packet_magic;
	packet.header.type = PacketType::packet_completed;
	packet.data.completed.result = result;

	KeLeaveGuardedRegion();

	return send(client_connection, &packet, sizeof(packet), 0) != SOCKET_ERROR;
}