/**
 * STRANDED by Neil a.k.a. LuKeM (NOTE: The file extension has been renamed to .cpp for
 * github syntax highlighting, but the actual extension is .nut)
 *
 * Move, build, and barricade. Scavenge for food and water. How long can you survive?
 */
//-------------------------------------------------------------------------------------------

IncludeScript("VSLib");


// Variables
::UsedSpawnLocations <- {};
::HasRelocated <- {};
::Food <- {};
::Water <- {};
::SurvRoundTime <- 0;
::LastWeaponFireTime <- {};
::SpawnTimes <- {};
::AttackingBots <- [];
::CanTrackPlayers <- true;
::CurrentDifficulty <- null;

::PLAYER_HUD <- {};
::PLAYER_HUD[1] <- { pos = {x=0, y=0}, hud = HUD_RIGHT_TOP };
::PLAYER_HUD[2] <- { pos = {x=150, y=0}, hud = HUD_LEFT_TOP };
::PLAYER_HUD[3] <- { pos = {x=300, y=0}, hud = HUD_RIGHT_BOT };
::PLAYER_HUD[4] <- { pos = {x=450, y=0}, hud = HUD_LEFT_BOT };

const WATER = 0;
const FOOD = 1;
const FOOD_MODEL = "models/props_junk/garbage_cerealbox01a_fullsheet.mdl";
const WATER_MODEL = "models/props_interiors/waterbottle.mdl";
const NUTRIENT_CHANCE = 4; /** Percent chance of a nutrient spawning */
const FOOD_CHANCE = 40; /** Percent chance of food spawning (water spawn rate thus will be 100 - FOOD_CHANCE) */
const MAX_NUTRIENTS = 40; /** Maximum number of nutrients that can exist on the map */

::SpawnedNutrients <- 0;
::InventoryHUD <- null; /** The HUD menu item */
::HasDisplayedWarning <- {}; /** Has a hint been displayed during the round? */

::RandSoundsList <- [
	"Event.HunterAlert",
	"Event.JockeyAlert",
	"Event.SmokerAlert",
	"Event.BoomerAlert",
	"Event.AmbientMob",
	"Event.ChargerAlert",
	"Event.LoneSurvivor",
	"Event.MobSignal1_Fairgrounds",
	"Event.ZombieChoir",
	"Zombie.Alert",
	"Zombie.Rage",
	"Zombie.RageAtVictim",
	"Zombie.BulletImpact",
	"Zombie.IgniteScream",
	"WoodenDoor.Break"
]

::WeaponSpawnNames <- {
	weapon_adrenaline_spawn = 0
	weapon_ammo_spawn = 0
	weapon_autoshotgun_spawn = 0
	weapon_chainsaw_spawn = 0
	weapon_defibrillator_spawn = 0
	weapon_first_aid_kit_spawn = 0
	weapon_gascan_spawn = 0
	weapon_grenade_launcher_spawn = 0
	weapon_hunting_rifle_spawn = 0
	weapon_item_spawn = 0
	weapon_melee_spawn = 0
	weapon_molotov_spawn = 0
	weapon_pain_pills_spawn = 0
	weapon_pipe_bomb_spawn = 0
	weapon_pistol_magnum_spawn = 0
	weapon_pistol_spawn = 0
	weapon_pumpshotgun_spawn = 0
	weapon_rifle_ak47_spawn = 0
	weapon_rifle_desert_spawn = 0
	weapon_rifle_m60_spawn = 0
	weapon_rifle_spawn = 0
	weapon_scavenge_item_spawn = 0
	weapon_shotgun_chrome_spawn = 0
	weapon_shotgun_spas_spawn = 0
	weapon_smg_silenced_spawn = 0
	weapon_smg_spawn = 0
	weapon_sniper_military_spawn = 0
	weapon_spawn = 0
	weapon_upgradepack_explosive_spawn = 0
	weapon_upgradepack_incendiary_spawn = 0
	weapon_vomitjar_spawn = 0
}


// Set up mutation options
MutationOptions <-
{
	ActiveChallenge = 1
	CommonLimit  = 200
	NumReservedWanderers = 200
	AlwaysAllowWanderers = true
	MegaMobSize  = 0
	MobSpawnSize = 0
	MobMaxSize = 0
	WanderingZombieDensityModifier = 0.01
	MaxSpecials  = 3
	TankLimit    = 0
	WitchLimit   = 0
	BoomerLimit  = 0
	ChargerLimit = 0
	HunterLimit  = 0
	JockeyLimit  = 0
	SpitterLimit = 0
	SmokerLimit  = 0
	SpecialRespawnInterval = 0
	TotalSpecials = 0
	cm_NoSurvivorBots = true
	AllowCrescendoEvents = false
	ZombieTankHealth = 300
	ClearedWandererRespawnChance = 100
}

DirectorOptions <-
{
	weaponsToRemove =
	{
		weapon_pistol = 0
		weapon_pistol_magnum = 0
		weapon_pipe_bomb = 0
		weapon_vomitjar = 0
		weapon_upgradepack_incendiary = 0
		weapon_upgradepack_explosive = 0
		weapon_ammo_pack = 0
		upgrade_item = 0
		ammo = 0
		weapon_bile_bomb = 0
		//weapon_molotov = 0
	}

	function AllowWeaponSpawn(classname)
	{
		if (classname in weaponsToRemove)
			return false;
		
		if (Utils.GetRandNumber(1, 5) >= 4)
			return false;
		
		return true;
	}

	function ShouldAvoidItem(classname)
	{
		if (classname in weaponsToRemove)
			return true;
		
		return false;
	}

	function GetDefaultItem(idx)
	{
		return 0;
	}
}

// Change difficulty related settings
function Notifications::OnDifficulty::ModDifficulty(diff)
{
	::CurrentDifficulty <- diff;
}

// Kill infected that have decap'd
function Notifications::OnInfectedHurt::DetectDecap(infected, attacker, params)
{
	local hitgroup = EasyLogic.GetEventInt(params, "hitgroup");
	if (attacker.GetPlayerType() == SURVIVOR && ((hitgroup >= 0 && hitgroup <= 3) || hitgroup == 6 || hitgroup == 7))
		infected.SetHealth(0);
}

// Calculated infacted damage and lowers it
function EasyLogic::OnDamage::infected ( victim, attacker, damageDone, damageTable )
{
	if (victim.GetIndex() == attacker.GetIndex() || !victim || !attacker || victim.GetIndex() <= 64)
		return;
	
	if (damageTable.DamageType == 8 || damageTable.DamageType == 2056)
		return damageDone;
	
	return floor(damageDone.tofloat() / 12.0);
}

// Leap of faith for infected
function Notifications::OnHurt::LeapOfFaith(victim, attacker, params)
{
	if (victim && attacker && victim.GetPlayerType() == SURVIVOR && victim.GetCurrentAttacker() != null && victim.IsPressingShove() && Utils.GetRandNumber(1, 6) == 3)
		attacker.Stagger();
}

// calculates player damage and multiplies it (to make infected much stronger)
function EasyLogic::OnDamage::player ( victim, attacker, damageDone, damageTable )
{
	if (!victim || !attacker || victim.GetIndex() == attacker.GetIndex() || damageTable.DamageType == 131072)
		return;
	
	switch(victim.GetPlayerType())
	{
		case SURVIVOR:
		{
			if (attacker.GetClassname() == "infected")
				return Utils.GetRandNumber(30, 60);
			
			else if (attacker.IsPlayerEntityValid())
			{
				switch (attacker.GetPlayerType())
				{
				case SURVIVOR:
					break;
				case SPITTER:
					return Utils.GetRandNumber(2, 5);
				case HUNTER:
					return Utils.GetRandNumber(5, 10);
				case CHARGER:
					return Utils.GetRandNumber(10, 15);
				case BOOMER:
					return Utils.GetRandNumber(1, 5);
				case SMOKER:
					return Utils.GetRandNumber(10, 15);
				case JOCKEY:
					return Utils.GetRandNumber(10, 15);
				default:
					return Utils.GetRandNumber(30, 60);
				}
			}
			
			break;
		}
	}
}

// Timer that resets Valve pickup system (in case players lose it due to bad pickup etc)
::ResetValvePickups <- function (player)
{
	if (!player.IsPlayerEntityValid())
		return false;
	
	player.BeginValvePickupObjects();
	player.ValvePickupPickupRange(200.0);
}


// Relocate survivor players on spawn
function Notifications::OnSpawn::RelocatePlayers ( player, params )
{
	if (player.GetPlayerType() != SURVIVOR)
		return;
	
	player.PlaySound("Event.SurvivalStart");
	Timers.AddTimer(10.0, true, ResetValvePickups, player);
	
	local pos = player.GetLocation();
	local ents = Objects.OfClassname("prop_physics");
	
	// Try to acquire a good, random spawn location
	for (local i = 0; i < 50; i++)
	{
		local vsent = ents[Utils.GetRandNumber(0, ents.len() - 1)];
		local entpos = vsent.GetLocation();
		local spawn = true;
		
		// prevent spawning too close to each other
		foreach (idx, n in UsedSpawnLocations)
		{
			local obj = Entity(idx);
			if (Utils.CalculateDistance(entpos, obj.GetLocation()) <= 1200.0)
			{
				spawn = false;
				break;
			}
		}
		
		if (spawn && Utils.CalculateDistance(pos, entpos) > 500.0 && !(vsent.GetIndex() in UsedSpawnLocations))
		{
			if ("LastRoundSpawns" in getroottable() && player.GetIndex() in LastRoundSpawns && Utils.CalculateDistance(entpos, LastRoundSpawns[player.GetIndex()]) < 500.0)
				continue;
			
			if (!("LastRoundSpawns" in getroottable()))
				::LastRoundSpawns <- {};
			
			::LastRoundSpawns[player.GetIndex()] <- entpos;
			UsedSpawnLocations[vsent.GetIndex()] <- true;
			player.SetLocation(entpos);
			
			// Check to make sure that a line can be traced from the eyes to the feet
			if (!player.CanTraceToLocation(player.GetLocation()))
			{
				printf("[Stranded] Invalid spawn detected for %s. Respawning...", player.GetName());
				continue;
			}
			
			return;
		}
	}
	
	local spawnPos = player.GetSpawnLocation();
	if (spawnPos)
		player.SetLocation(spawnPos);
	printf("[Stranded] Found no suitable spawn location for %s, spawning at default location", player.GetName());
}

::EndSimulatedPanicEvent <- function(params)
{
	local survs = Players.AliveSurvivors();
	
	foreach (infected in Objects.OfClassname("infected"))
	{
		local reset = true;
		local infloc = infected.GetLocation();
		
		foreach (surv in survs)
			if (infected.CanTraceToOtherEntity(surv))
			{
				reset = false;
				break;
			}
		
		if (reset)
			infected.BotReset();
	}
}

// Attract zombies when non-melee wep is fired
function OnGameEvent_weapon_fire ( params )
{
	local player = EasyLogic.GetPlayersFromEvent(params).entity;
	local idx = player.GetIndex();
	
	if (player.GetPlayerType() != SURVIVOR)
		return;
	
	local wep = player.GetActiveWeapon();
	local wepclass = wep.GetClassname();
	
	if (wepclass == "weapon_pipe_bomb")
	{
		player.AttachParticle("gas_fireball", 2.0);
		Utils.PlaySoundToAll("BaseGrenade.Explode");
		
		local infected = null;
		while (infected = Entities.FindByClassnameWithin(infected, "infected", player.GetLocation(), Utils.GetRandNumber(1200, 2000)))
		{
			local vsent = Entity(infected);
			vsent.BotReset();
		}
		
		::CanTrackPlayers <- false;
		
		Timers.AddTimer(6.0, false, @(n) ::CanTrackPlayers <- true);
		
		return;
	}
	
	if (!Utils.IsValidFireWeapon(wepclass) || wepclass.find("silenced") != null)
		return;
	
	if (!(idx in ::LastWeaponFireTime))
		::LastWeaponFireTime[idx] <- 0;
	
	if (Time() - ::LastWeaponFireTime[idx] < 30)
		return;
	
	printf("[Stranded] Simulated panic event started");
	
	::LastWeaponFireTime[idx] <- Time();
	Timers.AddTimerByName("PanicTimer" + idx, 45.0, false, ::EndSimulatedPanicEvent);
	
	Utils.PlaySoundToAll("Event.AmbientMob");
	
	local infected = null;
	while (infected = Entities.FindByClassnameWithin(infected, "infected", player.GetLocation(), Utils.GetRandNumber(584, 2500)))
	{
		local vsent = Entity(infected);
		vsent.BotAttack(player);
	}
	
	// Display a temporary hint
	if ( !(idx in HasDisplayedWarning) )
	{
		local gun = player.GetActiveWeapon();
		Utils.SetEntityHint( Entity(gun), "Loud noises attract zombies", "icon_skull", 400.0, false, 10.0, 0 );
		
		HasDisplayedWarning[idx] <- true;
	}
}

// Punish/notify players based on their current nutrient count
::CalcNutrientResults <- function(player, nutrient, tbl, idx, hud)
{
	tbl[idx] -= 1;
	
	// Notifications
	if (tbl[idx] == 5)
	{
		if (nutrient == WATER)
			Utils.SayToAll("%s is extremely thirsty.", player.GetName());
		else if (nutrient == FOOD)
			Utils.SayToAll("%s is extremely hungry.", player.GetName());
	}
	else if (tbl[idx] == 1)
	{
		if (nutrient == WATER)
			Utils.SayToAll("%s is about to die of thirst!", player.GetName());
		else if (nutrient == FOOD)
			Utils.SayToAll("%s is about to die of hunger!", player.GetName());
	}
	else if (tbl[idx] <= 0)
	{
		if (nutrient == WATER)
		{
			local fullhp = player.GetHealth();
			local newBuffer = fullhp - 15;
			
			player.SetReviveCount(1);
			player.SetRawHealth(0);
			player.SetHealthBuffer(newBuffer);
			
			if (newBuffer <= 0)
				player.Kill();
		}
		else if (nutrient == FOOD)
			player.Incapacitate();
	}
	
	if (Food[idx] <= 5 || Water[idx] <= 5)
		hud.StartBlinking();
	else
		hud.StopBlinking();
}

// Remove nutrients from total (this is a timer)
::RemoveNutrients <- function(params)
{
	local player = params.p;
	local nutrient = params.t;
	
	if (!player.IsPlayerEntityValid())
		return false;
	
	if (player.IsDead())
		return;
	
	local pid = player.GetIndex();
	local hud = HUD.Get(PLAYER_HUD[pid].hud);
	
	if (nutrient == FOOD)
	{
		CalcNutrientResults(player, FOOD, Food, pid, hud);
		hud.SetValue("food", (Food[pid] < 0 ? 0 : Food[pid]));
	}
	else if (nutrient == WATER)
	{
		CalcNutrientResults(player, WATER, Water, pid, hud);
		hud.SetValue("water", (Water[pid] < 0 ? 0 : Water[pid]));
	}
}

// Set up the player's HUD on spawn
function Notifications::OnPostSpawn::SetupPlayer(player, params)
{
	if (player.GetPlayerType() != SURVIVOR)
		return;
	
	local pid = player.GetIndex();
	
	printf("[Stranded] Player %d spawned", pid);
	
	Food[pid] <- 100;
	Water[pid] <- 100;
	
	local player_hud = HUD.Item("{name}\nFood: {food} Water: {water}\nVisibility: [{visible}]");
	player_hud.SetValue("name", player.GetName());
	player_hud.SetValue("food", Food[pid]);
	player_hud.SetValue("water", Water[pid]);
	player_hud.SetValue("visible", "");
	player_hud.AttachTo(PLAYER_HUD[pid].hud);
	player_hud.ChangeHUDNative(PLAYER_HUD[pid].pos.x, PLAYER_HUD[pid].pos.y, 140, 72, 640, 480);
	
	Timers.AddTimer(10.0 + Utils.GetRandNumber(-1, 3), true, RemoveNutrients, { p = player, t = FOOD });
	Timers.AddTimer(5.0 + Utils.GetRandNumber(-1, 3), true, RemoveNutrients, { p = player, t = WATER });
	
	player.SetFlashlight(false);
	player.Give("pipe_bomb");
	
	SpawnTimes[pid] <- Time();
	
	// Precache particles
	player.AttachParticle("gas_fireball", 0.1);
}

// Provide a !respawn function in case they get stuck somewhere
function ChatTriggers::respawn(player, args, text)
{
	local idx = player.GetIndex();
	if (idx in HasRelocated)
	{
		Utils.SayToAllDel("You have already respawned once this round.");
		return;
	}
	
	if ((Time() - SpawnTimes[idx]) > 60.0)
	{
		Utils.SayToAllDel("You can only respawn during the first 60 seconds after spawning.");
		return;
	}
	
	HasRelocated[idx] <- true;
	Notifications.OnSpawn.RelocatePlayers ( player, null );
	
	Utils.SayToAllDel("%s has relocated!", player.GetName());
}

// Returns the round time different
::GetRoundTimeDiff <- function()
{
	return floor(Time() - ::SurvRoundTime);
}

// Returns the current round time as a formatted string
::GetRoundTime <- function ()
{
	local timediff = GetRoundTimeDiff();
	local timetable = Utils.GetTimeTable( timediff );
	
	return format("%02d:%02d:%02d", timetable.hours, timetable.minutes, timetable.seconds);
}

// Gets the old round time
::GetOldRoundTime <- function()
{
	local fileName = SessionState.MapName + "_strandedtimes.dat";
	local oldTime = FileToString(fileName);
	
	// If the file doesn't exist, create it.
	if (oldTime == null)
	{
		StringToFile( fileName, "0" );
		oldTime = 0;
	}
	
	return oldTime.tointeger();
}

// Resets the countdown when survivors are dead, and saves if it's the best time
function Notifications::OnSurvivorsDead::ResetAndSave ( params )
{
	local fileName = SessionState.MapName + "_strandedtimes.dat";
	
	local oldTime = ::GetOldRoundTime();
	local newTime = ::GetRoundTimeDiff();
	
	if (newTime > 60)
		newTime += 60;
	
	local timetable = Utils.GetTimeTable( oldTime );
	local strOldTimeFormat = format("%02d:%02d:%02d", timetable.hours, timetable.minutes, timetable.seconds);
	
	// Save the best time
	if (newTime > oldTime)
	{
		Utils.PlaySoundToAll("Event.ScenarioWin_L4D1");
		Utils.SayToAll("CONGRATULATIONS! NEW HIGH SCORE!");
		Utils.SayToAll("Old survival time was %s.", strOldTimeFormat);
		Utils.SayToAll("New survival time is %s!", ::GetRoundTime());
		StringToFile( fileName, newTime.tostring() );
	}
	else
	{
		Utils.PlaySoundToAll("Event.ScavengeOvertimeStart");
		Utils.SayToAll("Sorry Survivors! You didn't beat your old time of %s.", strOldTimeFormat);
	}
	
	// Make the current time static as the screen fades out
	local timebox = HUD.Get(HUD_MID_TOP);
	if (timebox)
	{
		timebox.SetValue("time", ::GetRoundTime());
		timebox.StartBlinking();
	}
}

// Remove saferoom doors
function Notifications::OnRoundStart::RemoveSaferoomDoors ( params )
{
	printf("[Stranded] Locking ending saferoom doors");
	
	local function ForceCloseDoor()
	{
		foreach (door in Objects.OfClassname("prop_door_rotating_checkpoint"))
			if (GetFlowPercentForPosition(door.GetLocation(), false) > 50.0)
				door.Input("Close");
	}
	
	foreach (door in Objects.OfClassname("prop_door_rotating_checkpoint"))
	{
		if (GetFlowPercentForPosition(door.GetLocation(), false) > 50.0)
		{
			door.Input("Close");
			door.Input("Lock");
			door.ConnectOutput("OnOpen", ForceCloseDoor);
		}
	}
}

// Mod wep spawns to hold only 1 gun
function Notifications::OnRoundStart::ChangeWepSpawns ( params )
{
	// Wrap inside a timer so that the engine does not suspend the query
	local function TimerChangeWepSpawns(args)
	{
		foreach (idx, v in ::WeaponSpawnNames)
			foreach (wep in Objects.OfClassname(idx))
				wep.SetKeyValue("count", 1);
	}
	
	Timers.AddTimer(0.1, false, TimerChangeWepSpawns);
}

// Calculates misc slow poll things
::CalculateSlowPoll <- function(params)
{
	printf("%d commons left", Director.GetCommonInfectedCount());
	if (Director.GetCommonInfectedCount() <= floor(SessionOptions.CommonLimit / 1.5))
	{
		
	}
}

// Spawns Zombies
::ZombieSpawn <- function(params)
{
	if (::CurrentDifficulty == EASY)
	{
		printf("[Stranded] Low difficulty detected; SI will not spawn");
		return false;
	}
	
	local survs = Players.AliveSurvivors();
	local humanCount = survs.len();

	if (humanCount == 0)
		humanCount = 1;
		
	local randInf = 0;
	
	if (::CurrentDifficulty == NORMAL)
		randInf = Utils.GetRandNumber(HUNTER, CHARGER);
	else
		randInf = Utils.GetRandNumber(SMOKER, WITCH);
	
	if (survs.len() > 0)
		// Spawn a random zombie type near a random survivor
		Utils.SpawnZombieNearPlayer( Utils.GetRandValueFromArray(survs), randInf, 1000.0, 800.0 );
	
	local time = null;
	switch (humanCount)
	{
		case 1:
			time = 80;
			break;
		case 2:
			time = 60;
			break;
		case 3:
			time = 50;
			break;
		case 4:
			time = 40;
			break;
		default:
			time = 120.0;
			break;
	}
	
	Timers.AddTimer(time, false, ZombieSpawn);
}

// Attach hints to physics props, play sounds, and spawn SI
function Notifications::OnRoundStart::DoRandomStuff ( params )
{
	Timers.AddTimer(35.0, true, @(params) Utils.PlaySoundToAll(::RandSoundsList[Utils.GetRandNumber(0, ::RandSoundsList.len())]));
	
	Timers.AddTimer(100.0, false, ZombieSpawn);
	
	if (Convars.GetStr("z_difficulty") == EXPERT)
		Timers.AddTimer(900.0, true, @(n) Utils.SpawnZombieNearPlayer( Players.AnyAliveSurvivor(), TANK, 1000.0, 800.0 ) );
	
	Timers.AddTimer(10.0, true, CalculateSlowPoll);
	
	foreach (entity in Objects.OfClassname("prop_physics"))
		if (entity.GetName().find("ENT_NO_PICKUP") == null)
			Utils.SetEntityHint(entity, "Press USE to lift this object", "icon_arrow_plain_white_up", 200, true);
}

// Restarts the countdown at the beginning of a round
function Notifications::OnRoundStart::BeginCountdown ( params )
{
	printf("[Stranded] New Round Started");
	
	::SurvRoundTime <- Time();
	
	local hudtimer = HUD.Item("Survival Time:\n{time}");
	hudtimer.SetValue("time", GetRoundTime);
	hudtimer.AttachTo(HUD_MID_TOP);
	hudtimer.ChangeHUDNative(0, 90, 120, 47, 640, 480);
	hudtimer.SetTextPosition(TextAlign.Center);
}

// Spawns food water nearby
function VSLib::EasyLogic::Update::SpawnFoodWater()
{
	if (Utils.GetRandNumber(1, 100) <= NUTRIENT_CHANCE && ::SpawnedNutrients < MAX_NUTRIENTS)
	{
		local pos = Utils.GetNearbyLocationRadius( Players.AnyAliveSurvivor(), 800.0, 5000.0 );
		
		if (pos)
		{
			local item = null;
			local mdl = null;
			local txt = null;
			
			if (Utils.GetRandNumber(1, 100) < FOOD_CHANCE )
			{
				item = "food";
				mdl = FOOD_MODEL;
				txt = "Grab some food";
			}
			else
			{
				item = "water";
				mdl = WATER_MODEL;
				txt = "Pickup water";
			}
			
			::SpawnedNutrients++;
			
			local ent = Utils.SpawnInventoryItem( item, mdl, pos );
			local hint = Utils.SetEntityHint(ent, txt, "icon_alert", 512);
			ent.GetScriptScope()["hint"] <- hint;
		}
	}
}

// This updater is a "mini director" that calculates the following:
// 		- Alerts infected that are close to visible survivors
// 		- Global vision/hearing info is updated based on if survivors
//		  are crouching, shift-walking, etc and how the modifiers affect
//		  infected visibility.
//
function VSLib::EasyLogic::Update::MiniDirector()
{
	if (!::CanTrackPlayers)
		return;
	
	local survs = Players.AliveSurvivors();
	
	// Custom visibility detection
	foreach (surv in survs)
	{
		local pos = surv.GetLocation();
		local btnState = 2 - ((surv.IsPressingDuck() > 0).tointeger() + (surv.IsPressingWalk() > 0).tointeger());
		local seeRadius = 128.0 + 256.0 * btnState.tofloat();
		
		// Slow walk system
		if (btnState == 0)
			surv.SetFriction(1.25);
		else
			surv.SetFriction(1.0);
		
		// Update player visibility
		local hud = HUD.Get(PLAYER_HUD[surv.GetIndex()].hud);
		if (hud)
			hud.SetValue("visible", Utils.BuildProgressBar(12, btnState, 2));
		
		local infected = null;
		while (infected = Entities.FindByClassnameWithin (infected, "infected", pos, seeRadius))
		{
			local inf = Entity(infected);
			
			// Check if this infected can "see" the player
			if (inf.CanTraceToOtherEntity(surv, 40))
			{
				if (!(inf in AttackingBots))
				{
					AttackingBots.push(inf);
					inf.BotAttack(surv);
				}
			}
		}
	}
	
	// Reset all SI that are far away from all survivors
	foreach (SI in Players.Infected())
	{
		local CanSmell = false;
		
		foreach (surv in survs)
		{
			if (Utils.GetDistBetweenEntities(SI, surv) < 1400.0)
			{
				CanSmell = true;
				break;
			}
		}
		
		if (!CanSmell)
			SI.ChangeBotEyes(false);
		else
			SI.ChangeBotEyes(true);
	}
	
	// Reset attacking bots which are far away
	foreach (idx, infected in AttackingBots)
	{
		local reset = true;
		
		if (infected.IsEntityValid())
		{
			foreach (surv in survs)
				if (Utils.GetDistBetweenEntities(infected, surv) < 1500.0)
				{
					reset = false;
					break;
				}
		}
		else
		{
			AttackingBots.remove(idx);
			continue;
		}
		
		if (reset)
		{
			infected.BotReset();
			AttackingBots.remove(idx);
		}
	}
}

function Notifications::OnPickupInvItem::NotifyPickup(survivor, itemName, itemEnt)
{
	Utils.SayToAll("%s picked up some %s. Type !inv to access your inventory.", survivor.GetName(), itemName);
	
	local scope = ent.GetScriptScope();
	if ("hint" in scope)
		scope["hint"].Kill();
		
	::SpawnedNutrients--;
}

// Transfers an item from one inv to another
::TransferItem <- function (player, index, value)
{
	foreach (client in Players.All())
	{
		if (client.GetName() != value)
			continue;
		
		if (Utils.CalculateDistance(player.GetLocation(), client.GetLocation()) > 200.0)
		{
			Utils.SayToAll("Cannot give the item; the player is too far away.");
			break;
		}
		
		local inv1 = player.GetInventoryTable();
		inv1[::LastItemToTransfer] -= 1;
		
		local inv2 = client.GetInventoryTable();
		if (!(::LastItemToTransfer in inv2))
			inv2[::LastItemToTransfer] <- 0;
		inv2[::LastItemToTransfer] += 1;
		
		Utils.PlaySoundToAll("ambient.electrical_zap_5");
		Utils.SayToAll("%s gave %s to %s.", player.GetName(), ::LastItemToTransfer, client.GetName());
		
		break;
	}
}

// Constructs the Give menu
::GiveInvItem <- function (player, item)
{
	::LastItemToTransfer <- item;

	InventoryHUD <- HUD.Menu(player.GetName() + "\nGive to player:\n\n{options}");
	
	local count = 0;
	foreach (client in Players.AliveSurvivors())
		if (!client.IsBot() && client.GetName() != "" && client.GetIndex() != player.GetIndex())
		{
			count++;
			InventoryHUD.AddOption(client.GetName(), TransferItem);
		}
	
	InventoryHUD.AddOption("Cancel", HUDDoNothing);
	
	if (count > 0)
	{
		InventoryHUD.DisplayMenu(player, HUD_TICKER, true);
		InventoryHUD.OverrideButtons(BUTTON_WALK, BUTTON_SHOVE);
	}
	else
		Utils.SayToAllDel("Cannot give any item at the moment. Please try again later!");
}

// Does nothing.
::HUDDoNothing <- function(player, index, value)
{
}

// Consumes food from inventory
::EatFoodInv <- function(player, index, value)
{
	::Food[player.GetIndex()] <- 100;
	local inv = player.GetInventoryTable();
	inv["food"] -= 1;
	
	Utils.SayToAll("%s ate some food!", player.GetName());
}

// Opens Give menu
::GiveFoodInv <- function(player, index, value)
{
	GiveInvItem(player, "food");
}

// Open secondary Inv
::EatFood <- function(player, index, value)
{
	InventoryHUD <- HUD.Menu(player.GetName() + "\n[Item] Food:\n\n{options}");
	InventoryHUD.AddOption("Eat", EatFoodInv);
	InventoryHUD.AddOption("Give", GiveFoodInv);
	InventoryHUD.AddOption("Cancel", HUDDoNothing);
	InventoryHUD.DisplayMenu(player, HUD_TICKER, true);
	InventoryHUD.OverrideButtons(BUTTON_WALK, BUTTON_SHOVE);
}

// Opens Give menu
::GiveWaterInv <- function(player, index, value)
{
	GiveInvItem(player, "water");
}

// Consumes water from inv
::DrinkWaterInv <- function(player, index, value)
{
	::Water[player.GetIndex()] <- 100;
	local inv = player.GetInventoryTable();
	inv["water"] -= 1;
	
	// Reset B&W
	player.SetReviveCount(0);
	
	Utils.SayToAll("%s drank some water!", player.GetName());
}

// Opens drink menu
::DrinkWater <- function(player, index, value)
{
	InventoryHUD <- HUD.Menu(player.GetName() + "\n[Item] Water:\n\n{options}");
	InventoryHUD.AddOption("Drink", DrinkWaterInv);
	InventoryHUD.AddOption("Give", GiveWaterInv);
	InventoryHUD.AddOption("Cancel", HUDDoNothing);
	InventoryHUD.DisplayMenu(player, HUD_TICKER, true);
	InventoryHUD.OverrideButtons(BUTTON_WALK, BUTTON_SHOVE);
}

// Opens !inv
function EasyLogic::Triggers::inv ( player, args, text )
{
	if (::InventoryHUD != null)
	{
		Utils.SayToAllDel("%s: A round stats menu is already open.", player.GetName());
		return;
	}
	
	local inv = player.GetInventoryTable();
	
	InventoryHUD <- HUD.Menu("Inventory:\n" + player.GetName() + "\n\n{options}");
	
	local count = 0;
	foreach (item, value in inv)
	{
		if (value <= 0)
			continue;
		
		count++;
		
		if (item == "food")
			InventoryHUD.AddOption("Food (x" + value + ")", EatFood);
		else if (item == "water")
			InventoryHUD.AddOption("Water (x" + value + ")", DrinkWater);
	}
	
	if (count <= 0)
		InventoryHUD.AddOption("Empty Inventory", HUDDoNothing);
	else
		InventoryHUD.AddOption("Cancel", HUDDoNothing);
	
	InventoryHUD.DisplayMenu(player, HUD_TICKER, true);
	InventoryHUD.OverrideButtons(BUTTON_WALK, BUTTON_SHOVE);
	
	Utils.SayToAllDel("Hold the WALK key to select an option or hold MELEE to switch options.");
	Utils.SayToAllDel("%s is viewing his/her inventory.", player.GetName());
}

function ChatTriggers::supersecret(player, args, txt)
{
	local loc = player.GetLookingLocation();
	
	if (loc != null)
	{
		printf("DEBUG: SPAWNED");
		local eyes = player.GetEyeAngles();
		eyes.x = 0;
		DebugDrawBox(loc, Vector(10, 10, 20), Vector(-10, -10, 0), 255, 0, 0, 80, 0.5);
		Utils.SpawnPhysicsProp( "models/neil-119/woodenbox.mdl", loc + Vector(0, 0, 20), eyes, { health = 1, glowstate = 2 } );
	}
}