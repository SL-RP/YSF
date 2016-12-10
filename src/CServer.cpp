/*
*  Version: MPL 1.1
*
*  The contents of this file are subject to the Mozilla Public License Version
*  1.1 (the "License"); you may not use this file except in compliance with
*  the License. You may obtain a copy of the License at
*  http://www.mozilla.org/MPL/
*
*  Software distributed under the License is distributed on an "AS IS" basis,
*  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
*  for the specific language governing rights and limitations under the
*  License.
*
*  The Original Code is the YSI 2.0 SA:MP plugin.
*
*  The Initial Developer of the Original Code is Alex "Y_Less" Cole.
*  Portions created by the Initial Developer are Copyright (C) 2008
*  the Initial Developer. All Rights Reserved. The development was abandobed
*  around 2010, afterwards kurta999 has continued it.
*
*  Contributor(s):
*
*	0x688, balika011, Gamer_Z, iFarbod, karimcambridge, Mellnik, P3ti, Riddick94
*	Slice, sprtik, uint32, Whitetigerswt, Y_Less, ziggi and complete SA-MP community
*
*  Special Thanks to:
*
*	SA:MP Team past, present and future
*	Incognito, maddinat0r, OrMisicL, Zeex
*
*/

#include "main.h"

void CServer::Initialize(eSAMPVersion version)
{
	m_bInitialized = true;
	m_iTicks = 0;
	m_iTickRate = 5;
	m_bNightVisionFix = true;
	m_dwAFKAccuracy = 1500;

#ifndef _WIN32
	LoadTickCount();
#endif

	memset(&pPlayerData, NULL, sizeof(pPlayerData));
	bChangedVehicleColor.reset();
	memset(&COBJECT_AttachedObjectPlayer, INVALID_PLAYER_ID, sizeof(COBJECT_AttachedObjectPlayer));

	// Initialize addresses
	CAddress::Initialize(version);
	// Initialize SAMP Function
	CSAMPFunctions::PreInitialize();
	// Install pre-hooks
	InstallPreHooks();

	// Initialize default valid name characters
	for(BYTE i = '0'; i <= '9'; i++)
	{
		m_vecValidNameCharacters.insert(i);
	}
	for(BYTE i = 'A'; i <= 'Z'; i++)
	{
		m_vecValidNameCharacters.insert(i);
	}
	for(BYTE i = 'a'; i <= 'z'; i++)
	{
		m_vecValidNameCharacters.insert(i);
	}
	m_vecValidNameCharacters.insert({ ']', '[', '_', '$', '=', '(', ')', '@', '.' });
}

CServer::~CServer()
{
	for(int i = 0; i != MAX_PLAYERS; i++)
		RemovePlayer(i);

	SAFE_DELETE(pGangZonePool);
}

bool CServer::AddPlayer(int playerid)
{
	if(!pPlayerData[playerid])
	{
		pPlayerData[playerid] = new CPlayerData(static_cast<WORD>(playerid));
		return 1;
	}
	return 0;
}

bool CServer::RemovePlayer(int playerid)
{
	if(pPlayerData[playerid])
	{
		SAFE_DELETE(pPlayerData[playerid]);
		return 1;
	}
	return 0;
}

void CServer::Process()
{
	if(m_iTickRate == -1) return;

	if(++m_iTicks >= m_iTickRate)
	{
		m_iTicks = 0;
		for(WORD playerid = 0; playerid != MAX_PLAYERS; playerid++)
		{
			if(!IsPlayerConnected(playerid)) continue;
			
			// Process player
			pPlayerData[playerid]->Process();
		}
#ifdef NEW_PICKUP_SYSTEM
		if(CServer::Get()->pPickupPool)
			CServer::Get()->pPickupPool->Process();
#endif
	}
}

bool CServer::OnPlayerStreamIn(WORD playerid, WORD forplayerid)
{
	//logprintf("join stream zone playerid = %d, forplayerid = %d", playerid, forplayerid);
	PlayerID playerId = CSAMPFunctions::GetPlayerIDFromIndex(playerid);
	PlayerID forplayerId = CSAMPFunctions::GetPlayerIDFromIndex(forplayerid);
	
	// For security..
	if (playerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress || forplayerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress)
		return 0;

	if(!IsPlayerConnected(playerid) || !IsPlayerConnected(forplayerid))
		return 0;

	CObjectPool *pObjectPool = pNetGame->pObjectPool;
	for(WORD i = 0; i != MAX_OBJECTS; i++)
	{
		if(pPlayerData[forplayerid]->stObj[i].wAttachPlayerID == playerid && !pPlayerData[forplayerid]->bAttachedObjectCreated)
		{
			logprintf("should work");
			if(!pObjectPool->pPlayerObjects[forplayerid][i]) 
			{
				logprintf("YSF ASSERTATION FAILED <OnPlayerStreamIn> - m_pPlayerObjects = 0");
				return 0;
			}

			logprintf("attach objects i: %d, forplayerid: %d", i, forplayerid);
			// First create the object for the player. We don't remove it from the pools, so we need to send RPC for the client to create object
			RakNet::BitStream bs;
			bs.Write(pObjectPool->pPlayerObjects[forplayerid][i]->wObjectID); // m_wObjectID
			bs.Write(pObjectPool->pPlayerObjects[forplayerid][i]->iModel);  // iModel

			bs.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fX);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fY);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fZ);

			bs.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fX);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fY);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fZ);
			bs.Write(pObjectPool->pPlayerObjects[forplayerid][i]->fDrawDistance);
			bs.Write(pObjectPool->pPlayerObjects[forplayerid][i]->bNoCameraCol); 
			bs.Write((WORD)-1); // wAttachedVehicleID
			bs.Write((WORD)-1); // wAttachedObjectID
			bs.Write((BYTE)0); // dwMaterialCount

			CSAMPFunctions::RPC(&RPC_CreateObject, &bs, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, forplayerId, 0, 0);
			
			pPlayerData[forplayerid]->dwCreateAttachedObj = GetTickCount();
			pPlayerData[forplayerid]->dwObjectID = i;
			pPlayerData[forplayerid]->bAttachedObjectCreated = true;
			/*
			logprintf("join, modelid: %d, %d, %f, %f, %f, %f, %f, %f", pObjectPool->pPlayerObjects[forplayerid][i]->iModel,
				pPlayerData[forplayerid]->stObj[i].wAttachPlayerID,
				pPlayerData[forplayerid]->stObj[i].vecOffset.fX, pPlayerData[forplayerid]->stObj[i].vecOffset.fY, pPlayerData[forplayerid]->stObj[i].vecOffset.fZ,
				pPlayerData[forplayerid]->stObj[i].vecRot.fX, pPlayerData[forplayerid]->stObj[i].vecRot.fY, pPlayerData[forplayerid]->stObj[i].vecRot.fZ);
			*/
		}
	}
	return 1;
}

bool CServer::OnPlayerStreamOut(WORD playerid, WORD forplayerid)
{
	//logprintf("leave stream zone playerid = %d, forplayerid = %d", playerid, forplayerid);
	PlayerID playerId = CSAMPFunctions::GetPlayerIDFromIndex(playerid);
	PlayerID forplayerId = CSAMPFunctions::GetPlayerIDFromIndex(forplayerid);
	
	if (playerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress || forplayerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress)
		return 0;

	if(!IsPlayerConnected(playerid) || !IsPlayerConnected(forplayerid))
		return 0;

	CObjectPool *pObjectPool = pNetGame->pObjectPool;
	for(WORD i = 0; i != MAX_OBJECTS; i++)
	{
		if(pPlayerData[forplayerid]->stObj[i].wAttachPlayerID == playerid && pPlayerData[forplayerid]->bAttachedObjectCreated)
		{
			if(!pObjectPool->pPlayerObjects[forplayerid][i]) 
			{
				logprintf("YSF ASSERTATION FAILED <OnPlayerStreamOut> - m_pPlayerObjects = 0");
				return 0;
			}

			//logprintf("remove objects i: %d, forplayerid: %d", i, forplayerid);
			if(pPlayerData[playerid]->bAttachedObjectCreated)
			{
				pPlayerData[playerid]->DestroyObject(i);
			}
			pPlayerData[playerid]->dwCreateAttachedObj = 0;
			pPlayerData[forplayerid]->bAttachedObjectCreated = false;
			/*
			logprintf("leave, modelid: %d, %d, %f, %f, %f, %f, %f, %f", pObjectPool->pPlayerObjects[forplayerid][i]->iModel,
				pPlayerData[forplayerid]->stObj[i].wAttachPlayerID,
				pPlayerData[forplayerid]->stObj[i].vecOffset.fX, pPlayerData[forplayerid]->stObj[i].vecOffset.fY, pPlayerData[forplayerid]->stObj[i].vecOffset.fZ,
				pPlayerData[forplayerid]->stObj[i].vecRot.fX, pPlayerData[forplayerid]->stObj[i].vecRot.fY, pPlayerData[forplayerid]->stObj[i].vecRot.fZ);
			*/
		}
	}
	return 1;
}

void CServer::AllowNickNameCharacter(char character, bool enable)
{
	if (enable)
	{
		auto it = std::find(m_vecValidNameCharacters.begin(), m_vecValidNameCharacters.end(), character);
		if (it == m_vecValidNameCharacters.end())
		{
			m_vecValidNameCharacters.insert(character);
		}
	}
	else
	{
		auto it = std::find(m_vecValidNameCharacters.begin(), m_vecValidNameCharacters.end(), character);
		if (it != m_vecValidNameCharacters.end())
		{
			m_vecValidNameCharacters.erase(it);
		}
	}
}

bool CServer::IsNickNameCharacterAllowed(char character)
{
	auto it = std::find(m_vecValidNameCharacters.begin(), m_vecValidNameCharacters.end(), character);
	return (it != m_vecValidNameCharacters.end());
}

bool CServer::IsValidNick(char *szName)
{
	while (*szName)
	{
		if (IsNickNameCharacterAllowed(*szName))
		{
			szName++;
		}
		else
		{
			return false;
		}
	}
	return true;
}

void CServer::SetExtendedNetStatsEnabled(bool enable)
{
	if(CAddress::ADDR_GetNetworkStats_VerbosityLevel)
	{
		*(BYTE*)(CAddress::ADDR_GetNetworkStats_VerbosityLevel + 1) = enable ? 2 : 1;
		*(BYTE*)(CAddress::ADDR_GetPlayerNetworkStats_VerbosityLevel + 1) = enable ? 2 : 1;
	}
}

bool CServer::IsExtendedNetStatsEnabled(void)
{
	if(CAddress::ADDR_GetNetworkStats_VerbosityLevel)
	{
		return static_cast<int>(*(BYTE*)(CAddress::ADDR_GetNetworkStats_VerbosityLevel + 1) != 1);
	}
	return false;
}

WORD CServer::GetMaxPlayers()
{
	WORD count = 0;
	CPlayerPool *pPlayerPool = pNetGame->pPlayerPool;
	for (WORD i = 0; i != MAX_PLAYERS; i++)
		if (pPlayerPool->bIsNPC[i])
			count++;
	return static_cast<WORD>(CSAMPFunctions::GetIntVariable("maxplayers")) - count;
}

WORD CServer::GetPlayerCount()
{
	WORD count = 0;
	CPlayerPool *pPlayerPool = pNetGame->pPlayerPool;
	for (WORD i = 0; i != MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && !pPlayerPool->bIsNPC[i] && !pPlayerData[i]->bHidden)
			count++;
	return count;
}

WORD CServer::GetNPCCount()
{
	WORD count = 0;
	CPlayerPool *pPlayerPool = pNetGame->pPlayerPool;
	for (WORD i = 0; i != MAX_PLAYERS; i++)
		if (pPlayerPool->bIsPlayerConnected[i] && pPlayerPool->bIsNPC[i])
			count++;
	return count;
}

void CServer::RebuildSyncData(RakNet::BitStream *bsSync, WORD toplayerid)
{
	BYTE id;
	WORD playerid;

	bsSync->Read(id);
	bsSync->Read(playerid);

	//logprintf("RebuildSyncData pre %d - %d", id, playerid);
	if (!IsPlayerConnected(playerid) || !IsPlayerConnected(toplayerid)) return;

	//logprintf("RebuildSyncData %d - %d", id, playerid);
	switch (id)
	{
		case ID_PLAYER_SYNC:
		{
			CPlayer *p = pNetGame->pPlayerPool->pPlayer[playerid];
			WORD keys = 0;

			bsSync->Reset();
			bsSync->Write((BYTE)ID_PLAYER_SYNC);
			bsSync->Write(playerid);

			// LEFT/RIGHT KEYS
			if (p->syncData.wLRAnalog)
			{
				bsSync->Write(true);

				keys = p->syncData.wLRAnalog;
				keys &= ~pPlayerData[playerid]->wDisabledKeysLR;
				bsSync->Write(keys);
			}
			else
			{
				bsSync->Write(false);
			}

			// UP/DOWN KEYS
			if (p->syncData.wUDAnalog)
			{
				bsSync->Write(true);

				keys = p->syncData.wUDAnalog;
				keys &= ~pPlayerData[playerid]->wDisabledKeysUD;
				bsSync->Write(keys);
			}
			else
			{
				bsSync->Write(false);
			}

			// Keys
			keys = p->syncData.wKeys;
			keys &= ~pPlayerData[playerid]->wDisabledKeys;
			bsSync->Write(keys);

			// Position
			if (pPlayerData[toplayerid]->bCustomPos[playerid])
				bsSync->Write(*pPlayerData[toplayerid]->vecCustomPos[playerid]);
			else
				bsSync->Write((char*)&p->syncData.vecPosition, sizeof(CVector));

			// Rotation (in quaternion)
			if (pPlayerData[toplayerid]->bCustomQuat[playerid])
				bsSync->WriteNormQuat(pPlayerData[toplayerid]->fCustomQuat[playerid][0], pPlayerData[toplayerid]->fCustomQuat[playerid][1], pPlayerData[toplayerid]->fCustomQuat[playerid][2], pPlayerData[toplayerid]->fCustomQuat[playerid][3]);
			else
				bsSync->WriteNormQuat(p->syncData.fQuaternion[0], p->syncData.fQuaternion[1], p->syncData.fQuaternion[2], p->syncData.fQuaternion[3]);

			// Health & armour compression
			BYTE byteSyncHealthArmour = 0;
			if (p->syncData.byteHealth > 0 && p->syncData.byteHealth < 100)
			{
				byteSyncHealthArmour = ((BYTE)(p->syncData.byteHealth / 7)) << 4;
			}
			else if (p->syncData.byteHealth >= 100)
			{
				byteSyncHealthArmour = 0xF << 4;
			}

			if (p->syncData.byteArmour > 0 && p->syncData.byteArmour < 100)
			{
				byteSyncHealthArmour |= (BYTE)(p->syncData.byteArmour / 7);
			}
			else if (p->syncData.byteArmour >= 100)
			{
				byteSyncHealthArmour |= 0xF;
			}

			bsSync->Write(byteSyncHealthArmour);

			// Current weapon
			bsSync->Write(p->syncData.byteWeapon);

			// Special action
			bsSync->Write(p->syncData.byteSpecialAction);

			// Velocity
			bsSync->WriteVector(p->syncData.vecVelocity.fX, p->syncData.vecVelocity.fY, p->syncData.vecVelocity.fZ);

			// Vehicle surfing (POSITION RELATIVE TO CAR SYNC)
			if (p->syncData.wSurfingInfo)
			{
				bsSync->Write(true);
				bsSync->Write(p->syncData.wSurfingInfo);
				bsSync->Write(p->syncData.vecSurfing.fX);
				bsSync->Write(p->syncData.vecSurfing.fY);
				bsSync->Write(p->syncData.vecSurfing.fZ);
			}
			else
			{
				bsSync->Write(false);
			}

			// Animation
			if (p->syncData.dwAnimationData)
			{
				bsSync->Write(true);
				bsSync->Write((int)p->syncData.dwAnimationData);
			}
			else bsSync->Write(false);
			break;
		}
		case ID_VEHICLE_SYNC:
		{
			CPlayer *p = pNetGame->pPlayerPool->pPlayer[playerid];
			WORD keys = 0;

			bsSync->Reset();
			bsSync->Write((BYTE)ID_VEHICLE_SYNC);
			bsSync->Write(playerid);

			bsSync->Write(p->vehicleSyncData.wVehicleId);

			keys = p->vehicleSyncData.wLRAnalog;
			keys &= ~pPlayerData[playerid]->wDisabledKeysLR;
			bsSync->Write(keys);

			keys = p->vehicleSyncData.wUDAnalog;
			keys &= ~pPlayerData[playerid]->wDisabledKeysUD;
			bsSync->Write(keys);

			keys = p->vehicleSyncData.wKeys;
			keys &= ~pPlayerData[playerid]->wDisabledKeys;
			bsSync->Write(keys);

			bsSync->WriteNormQuat(p->vehicleSyncData.fQuaternion[0], p->vehicleSyncData.fQuaternion[1], p->vehicleSyncData.fQuaternion[2], p->vehicleSyncData.fQuaternion[3]);
			bsSync->Write((char*)&p->vehicleSyncData.vecPosition, sizeof(CVector));
			bsSync->WriteVector(p->vehicleSyncData.vecVelocity.fX, p->vehicleSyncData.vecVelocity.fY, p->vehicleSyncData.vecVelocity.fZ);
			bsSync->Write((WORD)p->vehicleSyncData.fHealth);

			// Health & armour compression
			BYTE byteSyncHealthArmour = 0;
			if (p->vehicleSyncData.bytePlayerHealth > 0 && p->vehicleSyncData.bytePlayerHealth < 100)
			{
				byteSyncHealthArmour = ((BYTE)(p->vehicleSyncData.bytePlayerHealth / 7)) << 4;
			}
			else if (p->vehicleSyncData.bytePlayerHealth >= 100)
			{
				byteSyncHealthArmour = 0xF << 4;
			}

			if (p->vehicleSyncData.bytePlayerArmour > 0 && p->vehicleSyncData.bytePlayerArmour < 100)
			{
				byteSyncHealthArmour |= (BYTE)(p->vehicleSyncData.bytePlayerArmour / 7);
			}
			else if (p->vehicleSyncData.bytePlayerArmour >= 100)
			{
				byteSyncHealthArmour |= 0xF;
			}

			bsSync->Write(byteSyncHealthArmour);
			bsSync->Write(p->vehicleSyncData.bytePlayerWeapon);

			if (p->vehicleSyncData.byteSirenState)
			{
				bsSync->Write(true);
			}
			else
			{
				bsSync->Write(false);
			}

			if (p->vehicleSyncData.byteGearState)
			{
				bsSync->Write(true);
			}
			else
			{
				bsSync->Write(false);
			}

			if (p->vehicleSyncData.fTrainSpeed)
			{
				bsSync->Write(true);
				bsSync->Write(p->vehicleSyncData.fTrainSpeed);
			}
			else
			{
				bsSync->Write(false);
			}

			if (p->vehicleSyncData.wTrailerID)
			{
				bsSync->Write(true);
				bsSync->Write(p->vehicleSyncData.wTrailerID);
			}
			else
			{
				bsSync->Write(false);
			}
			break;
		}
	}
}

void CServer::RebuildRPCData(BYTE uniqueID, RakNet::BitStream *bsSync, WORD playerid)
{
	switch (uniqueID)
	{
		case RPC_InitGame:
		{
			bool usecjwalk = static_cast<int>(pNetGame->bUseCJWalk) != 0;
			bool limitglobalchat = static_cast<int>(pNetGame->bLimitGlobalChatRadius) != 0;
			float globalchatradius = pNetGame->fGlobalChatRadius;
			float nametagdistance = pNetGame->fNameTagDrawDistance;
			bool disableenterexits = static_cast<int>(pNetGame->byteDisableEnterExits) != 0;
			bool nametaglos = static_cast<int>(pNetGame->byteNameTagLOS) != 0;
			bool manualvehengineandlights = static_cast<int>(pNetGame->bManulVehicleEngineAndLights) != 0;
			int spawnsavailable = pNetGame->iSpawnsAvailable;
			bool shownametags = static_cast<int>(pNetGame->byteShowNameTags) != 0;
			bool showplayermarkers = static_cast<int>(pNetGame->bShowPlayerMarkers) != 0;
			int onfoot_rate = CSAMPFunctions::GetIntVariable("onfoot_rate");
			int incar_rate = CSAMPFunctions::GetIntVariable("incar_rate");
			int weapon_rate = CSAMPFunctions::GetIntVariable("weapon_rate");
			int lacgompmode = CSAMPFunctions::GetIntVariable("lagcompmode");
			bool vehiclefriendlyfire = static_cast<int>(pNetGame->bVehicleFriendlyFire) != 0;

			CCallbackManager::OnPlayerClientGameInit(playerid, &usecjwalk, &limitglobalchat, &globalchatradius, &nametagdistance, &disableenterexits, &nametaglos, &manualvehengineandlights,
				&spawnsavailable, &shownametags, &showplayermarkers, &onfoot_rate, &incar_rate, &weapon_rate, &lacgompmode, &vehiclefriendlyfire);

			bsSync->Reset();
			bsSync->Write((bool)!!pNetGame->unklofasz);
			bsSync->Write((bool)usecjwalk);
			bsSync->Write((bool)!!pNetGame->byteAllowWeapons);
			bsSync->Write(limitglobalchat);
			bsSync->Write(globalchatradius);
			bsSync->Write((bool)!!pNetGame->byteStuntBonus);
			bsSync->Write(nametagdistance);
			bsSync->Write(disableenterexits);
			bsSync->Write(nametaglos);
			bsSync->Write(manualvehengineandlights);
			bsSync->Write(pNetGame->iSpawnsAvailable);
			bsSync->Write(playerid);
			bsSync->Write(shownametags);
			bsSync->Write((int)showplayermarkers);
			bsSync->Write(pNetGame->bTirePopping);
			bsSync->Write(pNetGame->byteWeather);
			bsSync->Write(pNetGame->fGravity);
			bsSync->Write((bool)!!pNetGame->bLanMode);
			bsSync->Write(pNetGame->iDeathDropMoney);
			bsSync->Write(false);
			bsSync->Write(onfoot_rate);
			bsSync->Write(incar_rate);
			bsSync->Write(weapon_rate);
			bsSync->Write((int)2);
			bsSync->Write(lacgompmode);

			const char* szHostName = CSAMPFunctions::GetStringVariable("hostname");
			if (szHostName)
			{
				size_t len = strlen(szHostName);
				bsSync->Write((BYTE)len);
				bsSync->Write(szHostName, len);
			}
			else
			{
				bsSync->Write((BYTE)0);
			}
			bsSync->Write((char*)&pNetGame->pVehiclePool, 212); // modelsUsed
			bsSync->Write((DWORD)vehiclefriendlyfire);
			break;
		}
	}
}