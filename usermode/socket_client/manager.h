#include <windows.h>
#include <cstdint>
#include "vector.h"
#include "driver.h"
#include <vector>

//#define ADDRESS_NETWORKMANAGER 0x51B8958
//#define ADDRESS_GAMEMANAGER 0x51CB748
//#define ADDRESS_GAMERENDERER 0x51CB590
//
//#define ADDRESS_LOCALPLAYERMANAGER 0x504B488
//
//#define OFFSET_GAMERENDERER_ENGINELINK 0xE0
//#define OFFSET_ENGINELINK_ENGINE 0x1C8
//#define OFFSET_ENGINE_CAMERA 0x8
//
//#define OFFSET_CAMERA_VIEWRIGHT 0xD0
//#define OFFSET_CAMERA_VIEWUP 0xE0
//#define OFFSET_CAMERA_VIEWFORWARD 0xF0
//#define OFFSET_CAMERA_VIEWTRANSLATION 0x100
//#define OFFSET_CAMERA_VIEWFOVX 0x110
//#define OFFSET_CAMERA_VIEWFOVY 0x124
//
//#define OFFSET_SKELETON_POSITION 0x700
//#define OFFSET_SKELETON_HEADPOSITION 0x6A0
//#define OFFSET_ENTITY_SKELETON 0x20
//#define OFFSET_ENTITY_ENTITYINFO 0x28
//#define OFFSET_ENTITY_PLAYERINFO 0xC8
//#define OFFSET_ENTITY_WEAPONINFO 0x98
//#define OFFSET_ENTITYINFO_POSITION 0x50
//#define OFFSET_ENTITYINFO_COMPONENTLIST 0xD8
//#define OFFSET_ENTITYINFO_WEAPONINDEX 0x18E
//#define OFFSET_COMPONENTLIST_HEALTHCOMPONENT 0x8
//#define OFFSET_HEALTHCOMPONENT_HEALTH 0x140
//#define OFFSET_GAMEMANAGER_ENTITYLIST 0x1C8
//
//#define OFFSET_GAME_MANAGER 0x51CB748
//#define OFFSET_GAME_MANAGER_ENTITY_COUNT 0x1D0
//#define OFFSET_GAME_MANAGER_ENTITY_LIST2 0x1D8
//
//#define OFFSET_ENTITY_COMPONENT 0x78 // Cav ESP 1
//#define OFFSET_ENTITY_COMPONENT_LIST 0x8 // Cav ESP 2
//#define OFFSET_ENTITY_COMPONENT_HEALTH 0x140
//
//#define OFFSET_SPOOF_SPECTATE 0x504DC10
//#define OFFSET_SPOOF_SPECTATE_ENABLE 0x5D
//
//#define OFFSET_ENVIRONMENT_AREA 0x5E3F4E8   // glow
//#define OFFSET_ENVIRONMENT_AREA_GLOW 0xB8
//#define OFFSET_ENVIRONMENT_AREA_GLOW_ON 0x114 // 0x11C; // 999.f
//#define OFFSET_ENVIRONMENT_AREA_START_DISTANCE  0x130 // 0x138; // 1.0f
//#define OFFSET_ENVIRONMENT_AREA_RELIEF 0x138 // 0x140; // 2.0f
//
//#define ADDRESS_FOVMANAGER 0x51cb730
//#define OFFSET_GAMEMANAGER_ISINGAME 0x32C
//#define OFFSET_STATUS_MANAGER 0x51CB6E0
//#define OFFSET_STATUS_MANAGER_CONTAINER 0x370
//#define OFFSET_STATUS_MANAGER_LOCALENTITY 0x28
//#define OFFSET_PROFILE_MANAGER 0x51b87b0
//#define OFFSET_LOCALPLAYERCONTAINER  0x5004828
//
//#define ENTITY_MARKER_VT_OFFSET 0x37BBA90
//#define ENTITY_MARKER_ENABLED_OFFSET 0x530
//
//#define OFFSET_ENTITY_PAWN 0x20
//#define OFFSET_ENTITY_PAWN_POSITION 0x6A0
//
//#define OFFSET_ENTITY_REPLICATION 0xC8
//#define OFFSET_ENTITY_REPLICATION_TEAM 0x190
//#define OFFSET_ENTITY_REPLICATION_NAME 0x1C0
//
//#define OFFSET_NET_MANAGER 0x51B8958  // not updated 4E96FB8
//
//#define OFFSET_CAMERA_MANAGER 0x5007FC0
//#define OFFSET_CAMERA_ENGINELINK 0x120
//#define OFFSET_CAMERA_ENGINE 0x1D8
//#define OFFSET_CAMERA_ENGINE_CAMERA 0x08
//#define OFFSET_TRIGGERBOT 0x504dc88
//#define OFFSET_TRIGGERBOT1 0x50
//#define OFFSET_TRIGGERBOT2 0x80
//#define OFFSET_TRIGGERBOT3 0x58
//#define OFFSET_TRIGGERBOT4 0x418
//#define OFFSET_TRIGGERBOT5 0x304
//
//
//#define OFFSET_CAMERA_RIGHT 0xB0
//#define OFFSET_CAMERA_UP 0xC0
//#define OFFSET_CAMERA_FORWARD 0xD0
//#define OFFSET_CAMERA_TRANSLATION 0xE0
//#define OFFSET_CAMERA_FOVX 0xF0
//#define OFFSET_CAMERA_FOVY 0x104
//
//#define OFFSET_NETWORK_MANAGER 0x51B8958

namespace offsets {
	// game manager
	constexpr uintptr_t game_manager = 0x51cb748;
	constexpr uintptr_t entity_count = 0x1D8;
	constexpr uintptr_t entity_list = 0x1C8;

	constexpr uintptr_t entity_component = 0x28;
	constexpr uintptr_t entity_health = 0x140;
	constexpr uintptr_t entity_head = 0x660;
	constexpr uintptr_t entity_pelvis = 0xFA0;
	constexpr uintptr_t entity_feet = 0x6C0;
	constexpr uintptr_t entity_rhand = 0x680;

	constexpr uintptr_t entity_replication = 0xa8;
	constexpr uintptr_t entity_replication_team = 0x19e;
	constexpr uintptr_t entity_replication_name = 0x1c8;

	// renderer manager
	constexpr uintptr_t renderer_manager = 0x51b8988;
	constexpr uintptr_t game_renderer_deref = 0x0;

	// status manager
	constexpr uintptr_t status_manager = 0x51cb6e0;
	constexpr uintptr_t status_manager_container = 0x68;
	constexpr uintptr_t status_manager_localentity = 0x28;

	constexpr uintptr_t entity_marker_vt_offset = 0x37bba90;
	constexpr uintptr_t entity_marker_enabled_offset = 0x530;

	// camera manager
	constexpr uintptr_t camera_manager = 0x51b8960;
	constexpr uintptr_t camera_enginelink = 0xe0;
	constexpr uintptr_t camera_engine = 0x1c8;
	constexpr uintptr_t camera_engine_camera = 0x08;

	constexpr uintptr_t camera_right = 0xd0;
	constexpr uintptr_t camera_up = 0xe0;
	constexpr uintptr_t camera_forward = 0xf0;
	constexpr uintptr_t camera_translation = 0x100;
	constexpr uintptr_t camera_fovx = 0x110;
	constexpr uintptr_t camera_fovy = 0x124;

	// network manager
	constexpr uintptr_t network_manager = 0x51b8958;

	// glow manager
	constexpr uintptr_t glow_manager = 0x5e3f4e8;

	// profile manager
	constexpr uintptr_t profile_manager = 0x51b87b0;

	// fov manager
	constexpr uintptr_t fov_manager = 0x51cb730;

	// interface manager 
	constexpr uintptr_t interface_manager = 0x51b87b0;

	// spoof specate manager
	constexpr uintptr_t spoof_spectate = 0x51b8840;

	// round manager
	constexpr uintptr_t round_manager = 0x51b8820;
}

namespace manager {
	uint32_t m_pid;
	uintptr_t m_base;
	SOCKET m_connection;

	unsigned long get_entity_count()
	{
		uint64_t game_manager = driver::read<uint64_t>(m_connection, m_pid, m_base + offsets::game_manager);

		if (!game_manager)
			return NULL;

		unsigned long entity_count = driver::read<unsigned long>(m_connection, m_pid, m_base + offsets::entity_count);

		if (!entity_count)
			return NULL;

		return entity_count;
	}

	uint64_t get_entity(unsigned int i)
	{
		uint64_t game_manager = driver::read<uint64_t>(m_connection, m_pid, m_base + offsets::game_manager);

		if (!game_manager)
			return NULL;

		uint64_t entity_list = driver::read<uint64_t>(m_connection, m_pid, game_manager + offsets::entity_list);

		if (!entity_list)
			return NULL;

		if (i > get_entity_count())
			return NULL;

		uint64_t entity = driver::read<uint64_t>(m_connection, m_pid, entity_list + (sizeof(PVOID) * i));

		if (!entity)
			return NULL;

		return entity;
	}

	std::vector<uint64_t> get_entities()
	{
		std::vector<uint64_t> entities;

		unsigned int count = get_entity_count();

		if (count > 255)
			return entities;

		for (unsigned int i = 0; i < count; i++)
		{
			uint64_t target_entity = get_entity(i);

			if (!target_entity)
				continue;

			entities.push_back(target_entity);
		}

		return entities;
	}

	INT8 get_entity_team(uint64_t entity)
	{
		if (!entity)
			return 0xFF;

		uint64_t replication = driver::read<uint64_t>(m_connection, m_pid, entity + offsets::entity_replication);

		if (!replication)
			return 0xFF;

		INT8 online_team_id = driver::read<INT8 >(m_connection, m_pid, replication + offsets::entity_replication_team);

		return online_team_id;
	}

	float get_entity_health(uint64_t i) {

		uint64_t game_manager = driver::read<uint64_t>(m_connection, m_pid, m_base + offsets::game_manager);

		if (!game_manager)
			return NULL;

		uint64_t entity_list = driver::read<uint64_t>(m_connection, m_pid, game_manager + offsets::entity_list);

		if (!entity_list)
			return NULL;

		if (i > get_entity_count())
			return NULL;

		uint64_t entity = driver::read<uint64_t>(m_connection, m_pid, entity_list + (sizeof(PVOID) * i));

		if (!entity)
			return NULL;

		uint64_t entity_health = driver::read<float>(m_connection, m_pid, entity + offsets::entity_health);

		if (!entity_health)
			return NULL;

		return entity_health;
	}
}
