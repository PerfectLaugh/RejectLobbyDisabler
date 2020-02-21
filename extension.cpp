#include "extension.h"
#include <ISDKTools.h>
#include <CDetour/detours.h>

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

RejectLobbyDisabler g_Extension;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_Extension);

IGameConfig *g_pGameConf = nullptr;

ISDKTools *sdktools = nullptr;
IServer *iserver = nullptr;

long long* g_pSvReservationCookie = nullptr;
CDetour *g_dtrIsExclusiveToLobbyConnections = nullptr;

bool g_bIntercept = false;
long long g_lPrevCookie = 0;

DETOUR_DECL_MEMBER0(IsExclusiveToLobbyConnections, bool)
{
	if (g_bIntercept && g_lPrevCookie != 0) {
		*g_pSvReservationCookie = 0;
	}
	return false;
}

SH_DECL_MANUALHOOK1(ProcessConnectionlessPacket, 0, 0, 0, bool, void*);

bool OnProcessConnectionlessPacket(void * pkt)
{
	g_bIntercept = true;
	if (g_lPrevCookie == 0) {
		g_lPrevCookie = *g_pSvReservationCookie;
	}
	else if (*g_pSvReservationCookie != 0 && g_lPrevCookie != *g_pSvReservationCookie) {
		g_lPrevCookie = *g_pSvReservationCookie;
	}
	RETURN_META_VALUE(MRES_IGNORED, false);
}

bool OnProcessConnectionlessPacketPost(void * pkt)
{
	g_bIntercept = false;
	if (g_lPrevCookie == 0) {
		RETURN_META_VALUE(MRES_IGNORED, nullptr);
	}
	*g_pSvReservationCookie = g_lPrevCookie;
	RETURN_META_VALUE(MRES_IGNORED, nullptr);
}

bool RejectLobbyDisabler::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	char conf_error[255];
	
	if (!gameconfs->LoadGameConfigFile("rejectlobbydisabler.games", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlength, "Could not read connecthook.games: %s", conf_error);
		return false;
	}

	sharesys->AddDependency(myself, "sdktools.ext", true, true);

	SM_GET_IFACE(SDKTOOLS, sdktools);
	iserver = sdktools->GetIServer();

	int offset = -1;

    if (!g_pGameConf->GetOffset("SvReservationCookie", &offset) || offset == -1)
    {
        smutils->Format(error, maxlength, "Failed to get SvReservationCookie offset.");
		return false;
    }
    g_pSvReservationCookie = (long long*)((char*)iserver + offset);
	
    offset = -1;
	if (!g_pGameConf->GetOffset("ProcessConnectionlessPacket", &offset) || offset == -1)
	{
		smutils->Format(error, maxlength, "Failed to get ProcessConnectionlessPacket offset.");
		return false;
	}
	SH_MANUALHOOK_RECONFIGURE(ProcessConnectionlessPacket, offset, 0, 0);

	CDetourManager::Init(g_pSM->GetScriptingEngine(), g_pGameConf);

	g_dtrIsExclusiveToLobbyConnections = DETOUR_CREATE_MEMBER(IsExclusiveToLobbyConnections, "CBaseServer::IsExclusiveToLobbyConnections");
	if (!g_dtrIsExclusiveToLobbyConnections)
	{
		smutils->Format(error, maxlength, "Detour failed: CBaseServer::IsExclusiveToLobbyConnections");
		return false;
	}

	SH_ADD_MANUALHOOK(ProcessConnectionlessPacket, iserver, SH_STATIC(OnProcessConnectionlessPacket), false);
	SH_ADD_MANUALHOOK(ProcessConnectionlessPacket, iserver, SH_STATIC(OnProcessConnectionlessPacketPost), true);
	g_dtrIsExclusiveToLobbyConnections->EnableDetour();

	return true;
}

void RejectLobbyDisabler::SDK_OnUnload()
{
	if (g_dtrIsExclusiveToLobbyConnections) {
		g_dtrIsExclusiveToLobbyConnections->Destroy();
		g_dtrIsExclusiveToLobbyConnections = nullptr;
	}
	SH_REMOVE_MANUALHOOK(ProcessConnectionlessPacket, iserver, SH_STATIC(OnProcessConnectionlessPacket), false);
	SH_REMOVE_MANUALHOOK(ProcessConnectionlessPacket, iserver, SH_STATIC(OnProcessConnectionlessPacketPost), true);
	if (g_pGameConf) {
		gameconfs->CloseGameConfigFile(g_pGameConf);
		g_pGameConf = nullptr;
	}
}
