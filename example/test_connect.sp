#pragma semicolon 1
#pragma newdecls required
#include <connecthook>

public Plugin myinfo = {
	name = "[ConnectHook] Sample Plugin",
	description = "",
	author = "PerfectLaugh",
	version = "0.3",
	url = ""
};

StringMap g_Transport;

public void OnPluginStart()
{
	RegConsoleCmd("test_transport", Test_Transport);
	ClientPreConnect_Hook(0, OnClientPreConnectEx);
	g_Transport = new StringMap();
}

public Action Test_Transport(int client, int args)
{
	char steamid[32];
	if (GetClientAuthId(client, AuthId_Steam2, steamid, sizeof(steamid))) {
		g_Transport.SetValue(steamid[8], true);
		ClientCommand(client, "retry");
	}
	return Plugin_Handled;
}

public EConnect OnClientPreConnectEx(int id, const char[] sName, const char[] sPassword, const char[] sIP, const char[] sSteam32ID, char sRejectReason[512])
{
	bool val;

	if (!g_Transport.GetValue(sSteam32ID[8], val)) {
		return k_OnClientPreConnectEx_Accept;
	}
	g_Transport.Remove(sSteam32ID[8]);
	Format(sRejectReason, sizeof(sRejectReason), "ConnectRedirectAddress:13.73.0.133:27017\n");
	return k_OnClientPreConnectEx_Reject;
}
