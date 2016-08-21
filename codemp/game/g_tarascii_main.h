#pragma once

#include "qcommon/q_shared.h" // For stuff like qboolean
#include "bg_public.h"
#include "g_local.h"

typedef struct tarasciiState_s
{
	qboolean AllowJoin; 
	int startingBarrel;
	int startTime;
	int winlossDelay;
	int concentration;
	vec3_t mapBoundPointsMin;
	vec3_t mapBoundPointsMax;
	vec3_t respawnBarrelPos[MAX_GENTITIES];
	qboolean explodeClicked[MAX_CLIENTS];
	qboolean timerSetOnce;
	int spawnedBarrels;
	int barrelsToRespawn;
	int dontRespawn[MAX_CLIENTS];
	int explodeClick[MAX_CLIENTS];
	int barrelTeamCounter;
	int humanTeamCounter;
	int sessionNumRounds;
	int clientVersions[MAX_CLIENTS];
	float clientVersionTimers[MAX_CLIENTS];
}tarasciiState_t;

typedef struct Vec3List_s
{
	vec3_t coordinate;
	struct Vec3List_s *next;
}Vec3List_t;

extern tarasciiState_t g_tState;
#define	MIN_WALK_NORMAL	0.7f
#define MIN_PLAYER_TO_START 2

void Tarascii_BarrelPlacement();
void Tarascii_Init(int levelTime);

void Tarascii_WriteSessionData();
void Tarascii_ReadSessionData();

qboolean Tarascii_CanPlayerJoin();
team_t Tarascii_GetTeam(gentity_t* ent);
void Tarascii_ClientSpawn(gentity_t* ent);

void Tarascii_RunFrame();
void Tarascii_ClientThink(gentity_t* ent);
void Tarascii_Disconnect(int clientNum);
void Tarascii_ExitCheck();

void Tarascii_BarrelNonSolid(int barrelNum);
void Tarascii_BarrelBound(gentity_t* ent);
void Tarascii_RespawnBarrels();

void Tarascii_ExplodeBarrel(gentity_t* ent);
void Tarascii_ResetEplodeClick(int clientNum);

void Tarascii_ClientBegin(gentity_t* ent);
void Tarascii_AddClientPluginVersion(int clientNum, int clientVersion);
void Tarascii_PluginIdentify( gentity_t *ent );
