/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// server.h

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../game/g_public.h"
#include "../game/bg_public.h"

//=============================================================================

#define	PERS_SCORE				0		// !!! MUST NOT CHANGE, SERVER AND
										// GAME BOTH REFERENCE !!!

#define	MAX_ENT_CLUSTERS	16

#ifdef USE_VOIP
#define VOIP_QUEUE_LENGTH 64

typedef struct voipServerPacket_s
{
	int	generation;
	int	sequence;
	int	frames;
	int	len;
	int	sender;
	int	flags;
	byte data[4000];
} voipServerPacket_t;
#endif

#ifdef USE_SKEETMOD
#define	MAX_SKEETS	128				// Max amount of skeets that will be animated in skeed mode

typedef struct skeetInfo_s {
	qboolean		valid;			// qtrue if this entity is a skeet
	vec3_t			origin;			// coordinates of the skeet spawn point
	int				shootTime;		// time when the skeet has been shot
	qboolean		moving;			// whether the skeet is movingin the air or not
} skeetInfo_t;
#endif

typedef struct svEntity_s {
	struct worldSector_s *worldSector;
	struct svEntity_s *nextEntityInWorldSector;
	
	entityState_t	baseline;		// for delta compression of initial sighting
	int			numClusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			lastCluster;		// if all the clusters don't fit in clusternums
	int			areanum, areanum2;
	int			snapshotCounter;	// used to prevent double adding from portal views
#ifdef USE_SKEETMOD
	skeetInfo_t	skeetInfo;
#endif
} svEntity_t;

typedef enum {
	SS_DEAD,			// no map loaded
	SS_LOADING,			// spawning level entities
	SS_GAME				// actively running
} serverState_t;

typedef struct {
	serverState_t	state;
	qboolean		restarting;			// if true, send configstring changes during SS_LOADING
	int				serverId;			// changes each server start
	int				restartedServerId;	// serverId before a map_restart
	int				checksumFeed;		// the feed key that we use to compute the pure checksum strings
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
	// the serverId associated with the current checksumFeed (always <= serverId)
	int				checksumFeedServerId;	
	int				snapshotCounter;	// incremented for each snapshot built
	int				timeResidual;		// <= 1000 / sv_frame->value
	int				nextFrameTime;		// when time > nextFrameTime, process world
	char			*configstrings[MAX_CONFIGSTRINGS];
	svEntity_t		svEntities[MAX_GENTITIES];

#ifdef USE_SKEETMOD
	svEntity_t		*skeets[MAX_SKEETS];
#endif

	char			*entityParsePoint;	// used during game VM init

	// the game virtual machine will update these on init and changes
	sharedEntity_t	*gentities;
	int				gentitySize;
	int				num_entities;		// current number, <= MAX_GENTITIES

	playerState_t	*gameClients;
	int				gameClientSize;		// will be > sizeof(playerState_t) due to game private data

	int				restartTime;
	int				time;
} server_t;





typedef struct {
	int				areabytes;
	byte			areabits[MAX_MAP_AREA_BYTES];		// portalarea visibility bits
	playerState_t	ps;
	int				num_entities;
	int				first_entity;		// into the circular sv_packet_entities[]
										// the entities MUST be in increasing state number
										// order, otherwise the delta compression will fail
	int				messageSent;		// time the message was transmitted
	int				messageAcked;		// time the message was acked
	int				messageSize;		// used to rate drop packets
} clientSnapshot_t;

typedef enum {
	CS_FREE,		// can be reused for a new connection
	CS_ZOMBIE,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	CS_CONNECTED,	// has been assigned to a client_t, but no gamestate yet
	CS_PRIMED,		// gamestate has been sent, but client hasn't sent a usercmd
	CS_ACTIVE		// client is fully in game
} clientState_t;

typedef struct netchan_buffer_s {
	msg_t			msg;
	byte			msgBuffer[MAX_MSGLEN];
#ifdef LEGACY_PROTOCOL
	char		clientCommandString[MAX_STRING_CHARS];	// valid command string for SV_Netchan_Encode
#endif
	struct netchan_buffer_s *next;
} netchan_buffer_t;

typedef struct client_s {
	clientState_t	state;
	char			userinfo[MAX_INFO_STRING];		// name, etc
	char			userinfobuffer[MAX_INFO_STRING]; //used for buffering of user info

	char			reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	int				reliableSequence;		// last added reliable message, not necessarily sent or acknowledged yet
	int				reliableAcknowledge;	// last acknowledged reliable message
	int				reliableSent;			// last sent reliable message, not necessarily acknowledged yet
	int				messageAcknowledge;

	int				gamestateMessageNum;	// netchan->outgoingSequence of gamestate
	int				challenge;

	usercmd_t		lastUsercmd;
	int				lastMessageNum;		// for delta compression
	int				lastClientCommand;	// reliable client message sequence
	char			lastClientCommandString[MAX_STRING_CHARS];
	sharedEntity_t	*gentity;			// SV_GentityNum(clientnum)
	char			name[MAX_NAME_LENGTH];			// extracted from userinfo, high bits masked

	int				deltaMessage;		// frame last client usercmd message
	int				nextReliableTime;	// svs.time when another reliable command will be allowed
	int				nextReliableUserTime; // svs.time when another userinfo change will be allowed
	int				lastPacketTime;		// svs.time when packet was last received
	int				lastConnectTime;	// svs.time when connection started
	int				lastSnapshotTime;	// svs.time of last sent snapshot
	qboolean		rateDelayed;		// true if nextSnapshotTime was set based on rate instead of snapshotMsec
	int				timeoutCount;		// must timeout a few frames in a row so debugging doesn't break
	clientSnapshot_t	frames[PACKET_BACKUP];	// updates can be delta'd from here
	int				ping;
	int				rate;				// bytes / second
	int				snapshotMsec;		// requests a snapshot every snapshotMsec unless rate choked
	int				pureAuthentic;
	qboolean  gotCP; // TTimo - additional flag to distinguish between a bad pure checksum, and no cp command at all
	netchan_t		netchan;
	int 			numcmds;			// number of client commands so far (in this time period), for sv_floodprotect

	// TTimo
	// queuing outgoing fragmented messages to send them properly, without udp packet bursts
	// in case large fragmented messages are stacking up
	// buffer them into this queue, and hand them out to netchan as needed
	netchan_buffer_t *netchan_start_queue;
	netchan_buffer_t **netchan_end_queue;

	qboolean	demo_recording;	// are we currently recording this client?
	fileHandle_t	demo_file;	// the file we are writing the demo to
	qboolean	demo_waiting;	// are we still waiting for the first non-delta frame?
	int		demo_backoff;	// how many packets (-1 actually) between non-delta frames?
	int		demo_deltas;	// how many delta frames did we let through so far?

#ifdef USE_VOIP
	qboolean hasVoip;
	qboolean muteAllVoip;
	qboolean ignoreVoipFromClient[MAX_CLIENTS];
	voipServerPacket_t *voipPacket[VOIP_QUEUE_LENGTH];
	int queuedVoipPackets;
	int queuedVoipIndex;
#endif

	int				oldServerTime;
	qboolean		csUpdated[MAX_CONFIGSTRINGS];
	
#ifdef LEGACY_PROTOCOL
	qboolean		compat;
#endif

#ifdef USE_AUTH
	char			auth[MAX_NAME_LENGTH];
#endif

#ifdef USE_SKEETMOD
	int lastEventSequence;
	int powerups[MAX_POWERUPS];
#endif

} client_t;

//=============================================================================


// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	2048
// Allow a certain amount of challenges to have the same IP address
// to make it a bit harder to DOS one single IP address from connecting
// while not allowing a single ip to grab all challenge resources
#define MAX_CHALLENGES_MULTI (MAX_CHALLENGES / 2)

typedef struct {
	netadr_t	adr;
	int			challenge;
	int			clientChallenge;		// challenge number coming from the client
	int			time;				// time the last packet was sent to the autherize server
	int			pingTime;			// time the challenge response was sent to client
	qboolean	wasrefused;
	qboolean	connected;
} challenge_t;

// this structure will be cleared only when the game dll changes
typedef struct {
	qboolean	initialized;				// sv_init has completed

	int			time;						// will be strictly increasing across level changes
	int			msgTime;					// will be used as precise sent time

	int			snapFlagServerBit;			// ^= SNAPFLAG_SERVERCOUNT every SV_SpawnServer()

	client_t	*clients;					// [sv_maxclients->integer];
	int			numSnapshotEntities;		// sv_maxclients->integer*PACKET_BACKUP*MAX_SNAPSHOT_ENTITIES
	int			nextSnapshotEntities;		// next snapshotEntities to use
	entityState_t	*snapshotEntities;		// [numSnapshotEntities]
	int			nextHeartbeatTime;
	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
	netadr_t	redirectAddress;			// for rcon return messages
	int			masterResolveTime[MAX_MASTER_SERVERS]; // next svs.time that server should do dns lookup for master server
} serverStatic_t;


// The value below is how many extra characters we reserve for every instance of '$' in a
// ut_radio, say, or similar client command.  Some jump maps have very long $location's.
// On these maps, it may be possible to crash the server if a carefully-crafted
// client command is sent.  The constant below may require further tweaking.  For example,
// a text of "$location" would have a total computed length of 25, because "$location" has
// 9 characters, and we increment that by 16 for the '$'.
#define STRLEN_INCREMENT_PER_DOLLAR_VAR 16

// Don't allow more than this many dollared-strings (e.g. $location) in a client command
// such as ut_radio and say.  Keep this value low for safety, in case some things like
// $location expand to very large strings in some maps.  There is really no reason to have
// more than 6 dollar vars (such as $weapon or $location) in things you tell other people.
#define MAX_DOLLAR_VARS 6

// When a radio text (as in "ut_radio 1 1 text") is sent, weird things start to happen
// when the text gets to be greater than 118 in length.  When the text is really large the
// server will crash.  There is an in-between gray zone above 118, but I don't really want
// to go there.  This is the maximum length of radio text that can be sent, taking into
// account increments due to presence of '$'.
#define MAX_RADIO_STRLEN 118

// Don't allow more than this text length in a command such as say.  I pulled this
// value out of my ass because I don't really know exactly when problems start to happen.
// This value takes into account increments due to the presence of '$'.
#define MAX_SAY_STRLEN 256

#define SERVER_MAXBANS	1024
// Structure for managing bans
typedef struct
{
	netadr_t ip;
	// For a CIDR-Notation type suffix
	int subnet;
	
	qboolean isexception;
} serverBan_t;

//=============================================================================

extern	serverStatic_t	svs;				// persistant server info across maps
extern	server_t		sv;					// cleared each map
extern	vm_t			*gvm;				// game virtual machine

extern	cvar_t	*sv_fps;
extern	cvar_t	*sv_timeout;
extern	cvar_t	*sv_zombietime;
extern	cvar_t	*sv_rconPassword;
extern	cvar_t	*sv_privatePassword;
extern	cvar_t	*sv_maxclients;
extern	cvar_t	*sv_privateClients;
extern	cvar_t	*sv_hostname;
extern	cvar_t	*sv_master[MAX_MASTER_SERVERS];
extern	cvar_t	*sv_reconnectlimit;
extern	cvar_t	*sv_showloss;
extern	cvar_t	*sv_padPackets;
extern	cvar_t	*sv_killserver;
extern	cvar_t	*sv_mapname;
extern	cvar_t	*sv_mapChecksum;
extern	cvar_t	*sv_serverid;
extern	cvar_t	*sv_minRate;
extern	cvar_t	*sv_maxRate;
extern	cvar_t	*sv_minPing;
extern	cvar_t	*sv_maxPing;
extern	cvar_t	*sv_gametype;
extern	cvar_t	*sv_pure;
extern	cvar_t	*sv_extraPure;
extern	cvar_t	*sv_extraPaks;
extern	cvar_t	*sv_newpurelist;
extern	cvar_t	*sv_floodProtect;
extern	cvar_t	*sv_lanForceRate;
extern	cvar_t	*sv_banFile;
extern	cvar_t	*sv_clientsPerIp;

extern	serverBan_t serverBans[SERVER_MAXBANS];
extern	int serverBansCount;

extern	cvar_t	*sv_demonotice;			// notice to print to a client being recorded server-side
extern	cvar_t	*sv_demofolder;			// define the server-side demo folder name
extern	cvar_t	*sv_autoRecordDemo;		// automatically create a server demo of every player that connects
extern	cvar_t	*sv_sayprefix;
extern	cvar_t	*sv_tellprefix;
extern	cvar_t	*sv_teamSwitch;			// allow players to switch teams (0, Default = players must wait 5 seconds to switch, 1 = no restriction)

#ifdef USE_VOIP
extern	cvar_t	*sv_voip;
extern	cvar_t	*sv_voipProtocol;
#endif

#ifdef USE_AUTH
extern	cvar_t	*sv_authServerIP;
extern	cvar_t	*sv_auth_engine;
#endif

#ifdef USE_SKEETMOD
extern	cvar_t	*sv_skeetshoot;			// enable/disable skeetshooting mod
extern	cvar_t	*sv_skeethitreport;		// report every skeet hit as server message
extern	cvar_t	*sv_skeethitsound;		// sound to play upon skeet hit
extern	cvar_t	*sv_skeetpoints;		// how many points for each skeet hit: if 0 will use a distance based point system
extern	cvar_t	*sv_skeetpointsnotify;	// notify each point scored to the client who performed the shot
extern	cvar_t	*sv_skeetprotect;		// protect hit/kill of non-skeet entities (i.e. players)
extern	cvar_t	*sv_skeetspeed;			// speed of each skeet
extern	cvar_t	*sv_skeetrotate;		// ROLL angle rotation (defaults to 0, range between -360 and +360)
extern	cvar_t	*sv_skeetfansize;		// spread of the skeet launcher (defaults to 144, range 0-360)
#endif

//===========================================================

//
// sv_main.c
//
typedef struct leakyBucket_s leakyBucket_t;
struct leakyBucket_s {
	netadrtype_t	type;

	union {
		byte	_4[4];
		byte	_6[16];
	} ipv;

	int						lastTime;
	signed char		burst;

	long					hash;

	leakyBucket_t *prev, *next;
};

extern leakyBucket_t outboundLeakyBucket;

qboolean	SVC_RateLimit( leakyBucket_t *bucket, int burst, int period );
qboolean	SVC_RateLimitAddress( netadr_t from, int burst, int period );

void		SV_FinalMessage (char *message);
void QDECL	SV_SendServerCommand( client_t *cl, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));


void		SV_AddOperatorCommands (void);
void		SV_RemoveOperatorCommands (void);


void		SV_MasterShutdown (void);
int			SV_RateMsec(client_t *client);


//
// sv_utils.c
//
int			SV_FindConfigstringIndex(char *name, int start, int max, qboolean create);
void QDECL	SV_LogPrintf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void		SV_SendScoreboardSingleMessageToAllClients(client_t *cl, playerState_t *ps);
void		SV_SendSoundToClient(client_t *cl, char *name);
int			SV_UnitsToMeters(float distance);
int			SV_XORShiftRand(void);
float		SV_XORShiftRandRange(float min, float max);
void		SV_XORShiftRandSeed(unsigned int seed);


//
// sv_skeetshoot.c
//
#ifdef USE_SKEETMOD
void		SV_SkeetInit(void);
void		SV_SkeetThink(void);
void		SV_SkeetLaunch(svEntity_t *sEnt, sharedEntity_t *gEnt);
void		SV_SkeetReset(svEntity_t *sEnt, sharedEntity_t *gEnt);
void		SV_SkeetRespawn(svEntity_t *sEnt, sharedEntity_t *gEnt);
void		SV_SkeetParseGameRconCommand(const char *text);
void		SV_SkeetParseGameServerCommand(int clientNum, const char *text);
void		SV_SkeetBackupPowerups(client_t *cl);
void		SV_SkeetRestorePowerups(client_t *cl);
void		SV_SkeetScore(client_t *cl, playerState_t *ps, trace_t *tr);
void		SV_SkeetClientEvents(client_t *cl);
qboolean	SV_SkeetShoot(client_t *cl, playerState_t *ps);
#endif

//
// sv_init.c
//
void		SV_SetConfigstring( int index, const char *val );
void		SV_GetConfigstring( int index, char *buffer, int bufferSize );
void		SV_UpdateConfigstrings( client_t *client );

void		SV_SetUserinfo( int index, const char *val );
void		SV_GetUserinfo( int index, char *buffer, int bufferSize );

void		SV_ChangeMaxClients( void );
void		SV_SpawnServer( char *server, qboolean killBots );



//
// sv_client.c
//
void		SV_GetChallenge(netadr_t from);

void		SV_DirectConnect( netadr_t from );

void		SV_ExecuteClientMessage( client_t *cl, msg_t *msg );
void		SV_UserinfoChanged( client_t *cl );

void		SV_ClientEnterWorld( client_t *client, usercmd_t *cmd );
void		SV_FreeClient(client_t *client);
void		SV_DropClient( client_t *drop, const char *reason );

#ifdef USE_AUTH
void		SV_Auth_DropClient(client_t *drop, const char *reason, const char *message);
#endif

void		SV_ExecuteClientCommand( client_t *cl, const char *s, qboolean clientOK );
void		SV_ClientThink (client_t *cl, usercmd_t *cmd);

int			SV_SendQueuedMessages(void);
void		SV_UpdateUserinfo_f( client_t *cl );


//
// sv_ccmds.c
//
void		SV_Heartbeat_f( void );
void		SVD_WriteDemoFile(const client_t*, const msg_t*);
void		SV_StartRecordOne(client_t *client, char *filename);

//
// sv_snapshot.c
//
void		SV_AddServerCommand( client_t *client, const char *cmd );
void		SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg );
void		SV_WriteFrameToClient (client_t *client, msg_t *msg);
void		SV_SendMessageToClient( msg_t *msg, client_t *client );
void		SV_SendClientMessages( void );
void		SV_SendClientSnapshot( client_t *client );
void		SV_CheckClientUserinfoTimer( void );

//
// sv_game.c
//
int	SV_NumForGentity( sharedEntity_t *ent );
sharedEntity_t *SV_GentityNum( int num );
playerState_t *SV_GameClientNum( int num );
svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt );
sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt );
void		SV_InitGameProgs ( void );
void		SV_ShutdownGameProgs ( void );
void		SV_RestartGameProgs( void );
qboolean	SV_inPVS (const vec3_t p1, const vec3_t p2);

//
// sv_bot.c
//
void		SV_BotFrame( int time );
int			SV_BotAllocateClient(void);
void		SV_BotFreeClient( int clientNum );

void		SV_BotInitCvars(void);
int			SV_BotLibSetup( void );
int			SV_BotLibShutdown( void );
int			SV_BotGetSnapshotEntity( int client, int ent );
int			SV_BotGetConsoleMessage( int client, char *buf, int size );

int			BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t *points);
void		BotImport_DebugPolygonDelete(int id);

void		SV_BotInitBotLib(void);

//============================================================
//
// high level object sorting to reduce interaction tests
//

void		SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void		SV_UnlinkEntity( sharedEntity_t *ent );
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void		SV_LinkEntity( sharedEntity_t *ent );
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->r.absmin and ent->r.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid


clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent );


void		SV_SectorList_f( void );


int			SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount );
// fills in a table of entity numbers with entities that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// The world entity is never returned in this list.


int			SV_PointContents( const vec3_t p, int passEntityNum );
// returns the CONTENTS_* value from the world and all entities at the given point.


void		SV_Trace( trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, int capsule );
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passEntityNum is explicitly excluded from clipping checks (normally ENTITYNUM_NONE)


void		SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask, int capsule );
// clip to a specific entity

//
// sv_net_chan.c
//
void		SV_Netchan_Transmit( client_t *client, msg_t *msg);
int			SV_Netchan_TransmitNextFragment(client_t *client);
qboolean	SV_Netchan_Process( client_t *client, msg_t *msg );
void		SV_Netchan_FreeQueue(client_t *client);
