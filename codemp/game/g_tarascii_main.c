#include "g_tarascii_main.h"
#include "qcommon/q_tarascii_shared.h"

tarasciiState_t g_tState;

qboolean Tarascii_CanPlayerJoin()
{
	return (qboolean)(g_tState.AllowJoin || tm_debugAllowJoin.integer != 0);
}

void Tarascii_Init(int levelTime)
{
	memset( &g_tState, 0, sizeof( g_tState ) );

	g_weaponDisable.integer = 524279;							// Has to be set this way or else gun pickups are still turned on during the first round.
	trap->Cvar_Set("timelimit", va("%i", timelimit.integer));	// Fix to make sure timelimit works when starting the map for the first time, seems to be a bug where it assumes 0 unless it gets set again.
	trap->Cvar_Set("disable_item_force_boon", "1");
	trap->Cvar_Set("disable_item_force_enlighten_light", "1");
	trap->Cvar_Set("disable_item_force_enlighten_dark", "1");
	trap->Cvar_Set("disable_item_medpak_instant", "1");
	trap->Cvar_Set("disable_item_medpac", "1");
	trap->Cvar_Set("disable_item_shield_sm_instant", "1");
	trap->Cvar_Set("disable_item_shield_lrg_instant", "1");
	trap->Cvar_Set("fraglimit", "0");
	trap->Cvar_Set("g_forcePowerDisable", "245757");			// Disable every forcepower except jump and seeing.
}

void Tarascii_ReadSessionData()
{
	int i;
	char sessionData[MAX_CVAR_VALUE_STRING] = {0};

	trap->Cvar_VariableStringBuffer( "sessionTarasciiNumRounds", sessionData, sizeof(sessionData) );
	g_tState.sessionNumRounds = atoi(sessionData);

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		trap->Cvar_VariableStringBuffer( va("sessionTarasciiHasClients%i", i), sessionData, sizeof(sessionData) );
		g_tState.clientVersions[i] = atoi(sessionData);
	}
}

void Tarascii_WriteSessionData() 
{
	int i;
	trap->Cvar_Set( "sessionTarasciiNumRounds", va("%i", g_tState.sessionNumRounds) );

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		trap->Cvar_Set( va("sessionTarasciiHasClients%i", i), va("%i", g_tState.clientVersions[i]) );
	}
}

void Tarascii_pickFirstBarrel()
{
	do 
	{
		g_tState.startingBarrel = random() * MAX_CLIENTS;
	} while (Q_stricmp(g_entities[g_tState.startingBarrel].classname,"player") != 0);
}

team_t Tarascii_GetTeam(gentity_t* ent)
{
	return (g_tState.startingBarrel == ent->s.clientNum || level.time > g_tState.winlossDelay) ? TEAM_RED : TEAM_BLUE;
}

void Tarascii_RunFrame()
{
	int i;
	int connectedClients = 0;

	g_tState.barrelTeamCounter = g_tState.humanTeamCounter = 0;

	// Counts players on each team and checks how many are still connecting.
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (Q_stricmp(g_entities[i].classname,"player") == 0)	
		{
			float pluginVersionTimer = g_tState.clientVersionTimers[i];

			if (g_entities[i].client->sess.sessionTeam == TEAM_RED)
			{
				g_tState.barrelTeamCounter++;
			}
			else if (g_entities[i].client->sess.sessionTeam == TEAM_BLUE)
			{
				g_tState.humanTeamCounter++;
			}

			if (g_entities[i].client->pers.connected == CON_CONNECTED)
			{
				connectedClients++;
			}

			// Sends message to clients who dont have a client plugin installed or it's outdated.
			if(pluginVersionTimer > 0)
			{
				if(level.time >= pluginVersionTimer)
				{
					if(g_tState.clientVersions[i] <= 0)
					{
						// Center print
						trap->SendServerCommand( i, "cp \"No plugin detected.\n");

						// Private message
						trap->SendServerCommand( i, va("chat \"Server: ^6No plugin detected. You're advised to install it for a smoother experience.\" %i", i));
					}

					if(g_tState.clientVersions[i] > 0 && g_tState.clientVersions[i] < CLIENT_VERSION)
					{
						// Center print
						trap->SendServerCommand( i, "cp \"Outdated plugin detected.\n");

						// Private message
						trap->SendServerCommand( i, va("chat \"Server: ^6Outdated plugin detected. Server is running version ^1%i^6. Updating the plugin is recommended.\" %i", CLIENT_VERSION, i));
					}

					// Stop the timer
					g_tState.clientVersionTimers[i] = 0;
				}
			}
		}
	}

	// Starts the timers for starting the game, only called once when there's enough players.
	// level.numConnectedClients doesn't work because it includes CON_CONNECTING too.
	if (!g_tState.timerSetOnce && connectedClients >= MIN_PLAYER_TO_START)
	{
		g_tState.timerSetOnce = qtrue;
		trap->SendServerCommand( -1, "cp \"We have enough players.\nThe game is about to start\"");
		g_tState.startTime = level.time + 5000;
		g_tState.winlossDelay = level.time + 7000;
	}


	// Checks if there are still at least 2 players connected and that the timer has been reached, then picks the starting barrel and allows players to join.
	if ( g_tState.AllowJoin == qfalse && g_tState.startTime != 0 && g_tState.startTime <= level.time)
	{
		if (connectedClients >= MIN_PLAYER_TO_START)
		{
			Tarascii_pickFirstBarrel();
			g_tState.AllowJoin = qtrue;
		}
		// If the timer has started but there is no longer enough players, let the timers reset.
		else
		{
			g_tState.timerSetOnce = qfalse;
		}
	}

	// Handling win/loss conditions that depend on either team having 0 members, winlossDelay added to make sure everyone has spawned first.
	if (level.numPlayingClients > 0 && g_tState.AllowJoin && level.time >= g_tState.winlossDelay)
	{
		if (g_tState.humanTeamCounter == 0)
		{
			static int delayWin = 0;
			if(delayWin == 0)
			{
				trap->SendServerCommand( -1, "cp \"Barrels have won!\"");
				delayWin = level.time + 5000;
			}
			else if (level.time >= delayWin)
			{
				BeginIntermission();
				delayWin = 0;
			}
		}

		if (g_tState.barrelTeamCounter == 0)
		{
			static int delayReset = 0;
			if(delayReset == 0)
			{
				trap->SendServerCommand( -1, "cp \"No barrel player, returning to intermission.\"");
				delayReset = level.time + 5000;
			}
			else if (level.time >= delayReset)
			{
				BeginIntermission();
				delayReset = 0;
			}
		}
	}

}

void Tarascii_SpawnBarrel(vec3_t incPos)
{
	char objectPathOne[MAX_TOKEN_CHARS];
	char objectPathTwo[MAX_TOKEN_CHARS];
	gentity_t *obj;
	int size, rnd;
	vec3_t angles;

	obj = G_Spawn();

	if ( !obj )
	{
		return;
	}

	obj->r.contents = CONTENTS_SOLID;
	obj->clipmask = 0;
	obj->s.eType = ET_MOVER;
	obj->classname = "tarasciiBarrel";

	// Set the model names and paths.
	Q_strncpyz(objectPathOne, "models/map_objects/factory/emod_big", sizeof(objectPathOne) );
	Q_strncpyz(objectPathTwo, "models/items/forcegem", sizeof(objectPathTwo) );

	obj->model = G_NewString(va ("%s.md3", objectPathTwo));//"*1";
	obj->model2 = G_NewString(va ("%s.md3", objectPathOne));

	obj->s.modelindex = G_ModelIndex( obj->model );
	obj->s.modelindex2 = G_ModelIndex( obj->model2 );


	// Move editor origin to pos
	VectorCopy( incPos, obj->s.origin );
	VectorCopy( obj->s.origin, obj->s.pos.trBase );
	VectorCopy( obj->s.origin, obj->r.currentOrigin );

	VectorClear( angles );
	VectorCopy( angles, obj->s.angles );
	VectorCopy( angles, obj->r.currentAngles );
	VectorCopy( obj->s.angles, obj->s.apos.trBase );

	rnd = crandom() * 1000;
	obj->r.currentAngles[1] = rnd;
	obj->s.angles[1] = rnd;
	obj->s.apos.trBase[1] = rnd;

	size = 10;
	VectorSet (obj->r.mins, -1*size, -1*size, 0);	// Hitbox sizes
	VectorSet (obj->r.maxs, size, size, size*5);

	VectorSet(obj->modelScale, 1, 1, 1);
	obj->s.iModelScale = 10;

	trap->LinkEntity((sharedEntity_t*)obj);
}

void addToList(Vec3List_t* list, vec3_t coordinate)
{
	Vec3List_t *pointer = NULL;
	pointer = (Vec3List_t*)malloc(sizeof(Vec3List_t));

	// Make sure we're on the last element in the list.
	while (list->next != NULL)
	{
		list = list->next;
	}

	if (pointer != NULL)
	{
		list->next = pointer;
		memset(pointer,0,sizeof(*pointer));
		VectorCopy(coordinate, pointer->coordinate);
	}
}

void Tarascii_RandomJKALocation(vec3_t outLocation)
{
	const int extraArea = 2000;
	float minRand, maxRand, range;
	int i;

	// Pick a random location within a random range.
	for (i = 0; i < 3; i++)
	{
		maxRand = g_tState.mapBoundPointsMax[i] + extraArea;
		minRand = g_tState.mapBoundPointsMin[i] - extraArea;
		range = maxRand - minRand;

		outLocation[i] = range * (random()) + minRand;
	}
}

void Tarascii_FindMapBounds()
{
	int i;
	int entOffset = MAX_CLIENTS + BODY_QUEUE_SIZE;
	vec3_t zeroVec = {0,0,0};

	// Set initial values for g_mapBoundPointsMin/Max.
	// Find the first map entity and use its location as initial mins/maxs for the map bounds.
	VectorCopy(g_entities[entOffset].s.origin, g_tState.mapBoundPointsMin);
	VectorCopy(g_tState.mapBoundPointsMin, g_tState.mapBoundPointsMax);

	// Compare remaining map entities to the initial values, storing new max and min coordinates.
	for (i = entOffset; i < level.num_entities; i++)
	{
		// Ignore entities that don't have coordinates assigned.
		int j;
		if (!VectorCompare(g_entities[i].s.origin, zeroVec))
		{
			for (j = 0; j < 3; j++)
			{
				if (g_entities[i].s.origin[j] > g_tState.mapBoundPointsMax[j])
				{
					g_tState.mapBoundPointsMax[j] = g_entities[i].s.origin[j];
				}

				if (g_entities[i].s.origin[j] < g_tState.mapBoundPointsMin[j])
				{
					g_tState.mapBoundPointsMin[j] = g_entities[i].s.origin[j];
				}
			}
		}
	}
}

void Tarascii_BarrelPlacement()
{
	trace_t groundTrace, traceWalls;
	gentity_t *avoidEnt;
	qboolean avoidZone, validSurface = qfalse;
	int i, numEnts, entOffset, avoidTouch[MAX_GENTITIES];
	int barrelBuffer = 100;	// Buffer at the end of placing barrels to leave some ents left for rockets shots, sounds etc.
	int maxEntities = (MAX_GENTITIES - level.num_entities - barrelBuffer);
	int noRoomRetryCounter = 0;
	int noRoomMaxRetry = 100; // How many tries until it stops trying to place more barrels.
	vec3_t downVec, avoidBoxMin, avoidBoxMax;
	vec3_t barrelBoundMin = {-10, -10, 0};
	vec3_t barrelBoundMax = {10, 10, 50};
	vec3_t barrelBoundLargerMin = {-15,-15,0};
	vec3_t barrelBoundLargerMax = {15,15,50};

	// Made my own linked list for storing barrel positions since C doesn't support 
	// them and I wanted to give it a try. Could be solved easier by just using an array[MAX_GENTITIES].
	Vec3List_t *barrelPositionList = (Vec3List_t*)malloc(sizeof(Vec3List_t));
	Vec3List_t *barrelPositionHead = barrelPositionList;
	memset(barrelPositionList, 0, sizeof(*barrelPositionList));
	
	Tarascii_FindMapBounds();	//Roughly finds the size of the level.

	while (g_tState.spawnedBarrels < maxEntities && noRoomRetryCounter < noRoomMaxRetry)
	{
		vec3_t spawnPoint;
		Tarascii_RandomJKALocation(spawnPoint);
		VectorCopy(spawnPoint, downVec);
		downVec[2] = -65536;

		// Initial trace using the random location chosen.
		trap->Trace(&groundTrace, spawnPoint, barrelBoundMin, barrelBoundMax, downVec, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);

		VectorClear(avoidBoxMin);
		VectorClear(avoidBoxMax);
		VectorAdd(groundTrace.endpos, barrelBoundLargerMin, avoidBoxMin);
		VectorAdd(groundTrace.endpos, barrelBoundLargerMax, avoidBoxMax);

		// Avoid Zone Check
		// Find all entities in the area, and make sure we're safe to spawn a barrel here.
		// In case of death triggers etc.
		numEnts = trap->EntitiesInBox( avoidBoxMin, avoidBoxMax, avoidTouch, MAX_GENTITIES );
		avoidZone = qfalse;
		for ( i = 0 ; i < numEnts ; i++ ) 
		{
			avoidEnt = &g_entities[avoidTouch[i]];
			if ( ( avoidEnt->r.contents & CONTENTS_TRIGGER ) ) 
			{
				// We don't want to place anything here.
				avoidZone = qtrue;
				break;
			}
		}

		if(avoidZone)
		{
			continue;
		}

		// Makes sure no barrel spawns too close to a spawn point so that spawning blue players can move.
		for (entOffset = MAX_CLIENTS + BODY_QUEUE_SIZE; entOffset < level.num_entities; entOffset++ )
		{
			avoidEnt = &g_entities[entOffset];
			if (!Q_strncmp(avoidEnt->classname, "info_player", 11))
			{
				if (Distance(groundTrace.endpos, avoidEnt->s.origin) < 40)
				{
					// We're too close to a spawn point, bail out.
					avoidZone = qtrue;
					break;
				}
			}
		}

		if(avoidZone)
		{
			continue;
		}


		// Checks if the slope of the surface is walkable, and if the surface is of a valid type.
		validSurface = (qboolean)((groundTrace.plane.normal[2] >= MIN_WALK_NORMAL) && !(groundTrace.surfaceFlags & (SURF_SKY | SURF_NODRAW)));

		if (validSurface)
		{
			// Check if the point overlaps any walls. Does not work on curved surfaces (patch surfaces)
			trap->Trace(&traceWalls, groundTrace.endpos, barrelBoundLargerMin, barrelBoundLargerMax, groundTrace.endpos, -1, MASK_SHOT, qfalse, 0, 0);

			if (traceWalls.fraction == 1.0)
			{
				qboolean isAvailable = qtrue;

				while (barrelPositionList->next != NULL) // Make sure we're on the last element in the list.
				{
					// Check if the point chosen is too close to one of the already stored positions.
					if (Distance(groundTrace.endpos, barrelPositionList->coordinate) < ((random() * 150) + 40))
					{
						isAvailable = qfalse;
						break;
					}
					barrelPositionList = barrelPositionList->next;
				}
				barrelPositionList = barrelPositionHead;

				if (isAvailable)
				{
					g_tState.spawnedBarrels++;
					noRoomRetryCounter = 0;
					addToList(barrelPositionList, groundTrace.endpos);
				}
				else
				{
					noRoomRetryCounter++;
				}
			}
			else
			{
				noRoomRetryCounter++;
			}
		}
	} // While end

	// Check if a % of the barrels should be removed prior to placement.
	if (tm_barrelDensity.integer != 100)
	{
		int removeNum = 0;
		float percentage = (float)1 - (float)tm_barrelDensity.integer/(float)100;

		Vec3List_t *deleteElement = NULL;

		removeNum = floor(g_tState.spawnedBarrels * percentage);

		// Delete a random barrel from the list while there are still barrels to remove.
		while (removeNum != 0)
		{
			int i;
			int limit = (rand() % g_tState.spawnedBarrels) - 1;

			for (i = 0; i <= limit; i++)
			{
				barrelPositionList = barrelPositionList->next;
			}

			deleteElement = barrelPositionList->next;
			barrelPositionList->next = barrelPositionList->next->next;
			free(deleteElement);
			barrelPositionList = barrelPositionHead;

			g_tState.spawnedBarrels--;
			removeNum--;
		}
	}

	// Placing barrels.
	while (barrelPositionList->next != NULL)
	{
		if (barrelPositionHead != barrelPositionList)
		{
			Tarascii_SpawnBarrel(barrelPositionList->coordinate);
		}
		barrelPositionList = barrelPositionList->next;
	}
	barrelPositionList = barrelPositionHead;

	free(barrelPositionHead);
}

void Tarascii_ExplodeBarrel(gentity_t* ent)
{
	if (!g_tState.explodeClicked[ent->s.clientNum])
	{
		g_tState.explodeClicked[ent->s.clientNum] = qtrue;
		g_tState.explodeClick[ent->s.clientNum] = level.time+1000;
		G_Sound( ent, CHAN_WEAPON, G_SoundIndex( "sound/weapons/thermal/warning.wav" ) );
	}
}

void Tarascii_ResetEplodeClick(int clientNum)
{
	g_tState.explodeClicked[clientNum] = qfalse;
}

void Tarascii_ClientThink(gentity_t* ent)
{
	// Sets team if allowed to join.
	if ( g_tState.AllowJoin == qtrue && g_tState.startTime != 0 && g_tState.startTime <= level.time && ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		SetTeam(ent, NULL);
	}
	
	// Tells each player the round timelimit.
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && level.time >= g_tState.startTime && level.time <= g_tState.startTime + 100)
	{
 		trap->SendServerCommand( ent->s.clientNum, va("cp \"%s minute round.\"", timelimit.string));
	}

	// Checks if the barrel has clicked to explode and if it's time to explode yet.
	if (g_tState.explodeClick[ent->s.clientNum] != 0)
	{
		if (level.time >= g_tState.explodeClick[ent->s.clientNum])
		{
			G_Damage(ent, ent, ent, ent->r.currentAngles, ent->r.currentOrigin, 999, 0, MOD_THERMAL);
			g_tState.explodeClick[ent->s.clientNum] = 0;
		}
		if (ent->health <= 0)
		{
			g_tState.explodeClick[ent->s.clientNum] = 0;
		}
	}

	// Clientplugin turns off zoom completely but this is still needed for clients without the clientplugin.
	ent->client->ps.zoomMode = 0;
}

void Tarascii_BarrelBound(gentity_t* ent)
{
	// Best match for player barrels hitbox to match nonplayer barrels.
	vec3_t boxMin = {-10, -10, -24};
	vec3_t boxMax = {10, 10, 26}; 


	if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		ent->client->ps.crouchheight = 20;
		ent->client->ps.standheight = 20;

		VectorCopy(boxMin,ent->r.mins);
		VectorCopy(boxMax,ent->r.maxs);

		VectorCopy(boxMin,pm->mins);
		VectorCopy(boxMax,pm->maxs);
	}
}

void Tarascii_ClientSpawn(gentity_t* ent)
{
	int barrelNum = 0;
	vec3_t respawnHeightOffset;

	if ( level.gametype == GT_TEAM ) 
	{
		// Barrel Player Setup.
		if ( ent->client->sess.sessionTeam == TEAM_RED )
		{
			ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH] * 1.00;
			ent->client->ps.stats[STAT_ARMOR] = 0;
			ent->client->ps.weapon = WP_MELEE;
			ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
			ForceSeeing(ent);

			// Invisibility
			ent->client->ps.eFlags |= EF_NODRAW;	// Turns off player model.
			ent->client->ps.powerups[PW_CLOAKED] = Q3_INFINITE;	// Turns off mouseover target.

			// If there are no available barrels, forcefully pick the first pending barrel to respawn and respawn it. 
			// Then remove it from the respawn queue and have it ready to be randomly selected again because
			// I'm not storing the barrel ID.
			if (g_tState.barrelsToRespawn >= g_tState.spawnedBarrels)		
			{												
				vec3_t zeroPos = {0,0,0};
				int i;
				for (i = 0; i < MAX_GENTITIES; i++)
				{
					if (VectorCompare(g_tState.respawnBarrelPos[i], zeroPos) == 0)
					{
						int j;
						for (j = 0; j < MAX_CLIENTS; j++)
						{
							if (g_tState.dontRespawn[j] != i)
							{
								Tarascii_SpawnBarrel(g_tState.respawnBarrelPos[i]);
								VectorClear(g_tState.respawnBarrelPos[i]);
								g_tState.barrelsToRespawn--;
								break;
							}
						}
					}
				}
			}

			// Pick a random barrel to give the player.
			while (qtrue)
			{
				barrelNum = random() * (MAX_GENTITIES - (MAX_CLIENTS + BODY_QUEUE_SIZE)) + (MAX_CLIENTS + BODY_QUEUE_SIZE);

				if (Q_stricmp(g_entities[barrelNum].classname,"tarasciiBarrel") == 0)
				{
					int i;
					qboolean foundOccupied = qfalse;
					// Checks if the randomly chosen entity is already attached to a player, select a new one if so.
					for (i = 0; i < MAX_CLIENTS; i++)		
					{
						if (g_tState.dontRespawn[i] == barrelNum)
						{
							foundOccupied = qtrue;
							break;
						}
					}
					if (!foundOccupied)
					{
						break;
					}
				}
			}

			// Teleport player to selected barrel.
			VectorCopy(g_entities[barrelNum].s.origin,respawnHeightOffset);
			respawnHeightOffset[2] += 23;
			TeleportPlayer( ent, respawnHeightOffset, g_entities[barrelNum].s.angles );
			SetClientViewAngle( ent, g_entities[barrelNum].s.angles );

			// Use playerstate->duelindex to store which barrel is attached to player for networking purposes. This is because of the engine's limited network modding support.
			ent->playerState->duelIndex = g_tState.dontRespawn[ent->s.clientNum] = barrelNum;
			VectorCopy(g_entities[barrelNum].s.origin, g_tState.respawnBarrelPos[barrelNum]);
			g_tState.barrelsToRespawn++;

			g_entities[barrelNum].s.isPortalEnt = qtrue;	// Fixed a major bug that caused barrels to be culled when too far from their initial spawn point because of map area portals.
			ent->playerState->duelTime = 1;					// Fix to prevent looping explosion damage. Re-using duelTime.
		}
		else if (ent->client->sess.sessionTeam == TEAM_BLUE)
		{
			ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH];
			ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
			ent->client->ps.weapon = WP_DISRUPTOR;
			ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_DISRUPTOR);
			ent->client->ps.ammo[AMMO_POWERCELL] = 9999;
			ent->s.isPortalEnt = qtrue; // Keeps blue team from being culled because of area portals.
		}
	}
}

void Tarascii_RespawnBarrels()
{
	vec3_t zeroPos = {0,0,0};
	qboolean respawn;
	
	// Respawns only if there's currently more barrels to respawn than being occupied.
	if (g_tState.barrelsToRespawn > g_tState.barrelTeamCounter)
	{
		int i;
		for (i = 0; i < MAX_GENTITIES; i++)
		{

			if (VectorCompare(g_tState.respawnBarrelPos[i], zeroPos) == 0)
			{
				int j;
				respawn = qtrue;

				// If the barrel is being occupied by a barrel player, dont respawn it.
				for (j = 0; j < MAX_CLIENTS; j++)
				{
					if (g_tState.dontRespawn[j] == i)
					{
						respawn = qfalse;
						break;
					}
				}

				if (respawn)
				{
					Tarascii_SpawnBarrel(g_tState.respawnBarrelPos[i]);
					VectorClear(g_tState.respawnBarrelPos[i]);
					g_tState.barrelsToRespawn--;
				}

				if (g_tState.barrelsToRespawn == g_tState.barrelTeamCounter)
				{
					break;
				}
			}
		}
	}
}

void Tarascii_BarrelNonSolid(int barrelNum)
{
	g_entities[barrelNum].r.contents = 0;
}

void Tarascii_Disconnect(int clientNum)
{
	gentity_t *ent;

	ent = &g_entities[clientNum];

	// Cleanup barrel attachment.
	if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		G_FreeEntity(&g_entities[ent->playerState->duelIndex]);
		g_tState.dontRespawn[clientNum] = 0;
		g_tState.barrelsToRespawn--;
	}

	g_tState.clientVersions[clientNum] = -1;
}

void ExitLevel (void);
void Tarascii_ExitCheck()
{
	// Replays the same map for the set amount of rounds and then moves on to the next level in the queue.
	if (g_tState.sessionNumRounds != tm_rounds.integer)
	{
		g_tState.sessionNumRounds++;
		trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
		return;
	}
	else
	{
		g_tState.sessionNumRounds = 0;
		ExitLevel();
		return;
	}
}

void Tarascii_AddClientPluginVersion(int clientNum, int clientVersion)
{
	g_tState.clientVersions[clientNum] = clientVersion;
}

void Tarascii_ClientBegin(gentity_t* ent)
{
	// Start the timer when a client joins to check if they should recieve a clientplugin message.
	g_tState.clientVersionTimers[ent-g_entities] = level.time + 15000;
}

void Tarascii_PluginIdentify( gentity_t *ent )
{
	if(trap->Argc() == 2)
	{
		char sarg[MAX_STRING_CHARS];
		int clientVersionNumber = -1;

		trap->Argv( 1, sarg, sizeof( sarg ) );

		if(Q_isanumber(sarg))
		{
			clientVersionNumber = atoi(sarg);
			Tarascii_AddClientPluginVersion(ent-g_entities, clientVersionNumber);
		}
	}
}
