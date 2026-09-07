#ifndef ART_DEFS_H
#define ART_DEFS_H

namespace fallout {

constexpr inline int frameIdFromFid(int fid)
{
    return fid & 0xFFF;
}

constexpr inline int frameIdFromPid(int pid)
{
    return pid & 0xFFFFFF;
}

enum HeadFrameId : int {
    HEAD_INVALID = -1,
    HEAD_NONE,
    HEAD_MARCUS,
    HEAD_MYRON,
    HEAD_ELDER,
    HEAD_LYNETTE,
    HEAD_HAROLD,
    HEAD_TANDI,
    HEAD_COM_OFFICER,
    HEAD_SULIK,
    HEAD_PRESIDENT,
    HEAD_HAKUNIN,
    HEAD_BOSS,
    HEAD_DYING_HAKUNIN,
};

inline bool headFrameIdIsValid(int head)
{
    return head >= HEAD_NONE;
}

inline HeadFrameId headFrameIdFromFid(int fid)
{
    return static_cast<HeadFrameId>(frameIdFromFid(fid));
}

enum HeadAnimation : int {
    HEAD_ANIMATION_VERY_GOOD_REACTION = 0,
    HEAD_ANIMATION_GOOD = 1,
    HEAD_ANIMATION_GOOD_TO_NEUTRAL = 2,
    HEAD_ANIMATION_NEUTRAL_TO_GOOD = 3,
    HEAD_ANIMATION_NEUTRAL = 4,
    HEAD_ANIMATION_NEUTRAL_TO_BAD = 5,
    HEAD_ANIMATION_BAD_TO_NEUTRAL = 6,
    HEAD_ANIMATION_BAD = 7,
    HEAD_ANIMATION_VERY_BAD_REACTION = 8,
    HEAD_ANIMATION_GOOD_PHONEMES = 9,
    HEAD_ANIMATION_NEUTRAL_PHONEMES = 10,
    HEAD_ANIMATION_BAD_PHONEMES = 11,
};

enum HeadFidget : int {
    FIDGET_INVALID = -1,
    FIDGET_GOOD = 1,
    FIDGET_NEUTRAL = 4,
    FIDGET_BAD = 7,
};

inline HeadFidget headFidgetFromFid(int fid)
{
    int fidget = (fid & 0xFF0000) >> 16;
    return static_cast<HeadFidget>(fidget);
}

inline HeadAnimation headAnimationFromHeadFidget(HeadFidget fidget)
{
    return fidget != FIDGET_INVALID ? static_cast<HeadAnimation>(fidget) : HEAD_ANIMATION_VERY_GOOD_REACTION;
}

enum class BackgroundFrameId : int {
    Invalid = -1, // invalid frame id
    Reserved = 0, // reserved.frm - not working in game
    Background = 1, // back1.frm - not working in game
    RustyMetal = 2, // rstymetl.frm
    Hub = 3, // hub.frm
    Necropolis = 4, // necro.frm
    Brotherhood = 5, // bhood.frm
    MilitaryBase = 6, // military.frm
    JunkTown = 7, // junktown.frm
    Cathedral = 8, // cath.frm
    ShadySands = 9, // shady.frm
    Vault = 10, // vault.frm
    Master = 11, // master.frm
    Followers = 12, // follow.frm
    Raiders = 13, // raider.frm
    Cave = 14, // cave0001.frm
    Enclave = 15, // enclave.frm
    Wasteland = 16, // wastelnd.frm
    EnclaveBoss = 17, // boss.frm
    President = 18, // pres.frm
    Tent = 19, // tent.frm
    Adobe = 20, // adobe.frm
};

enum DudeNativeLook : int {
    // Hero looks as one the tribals (before finishing Temple of Trails).
    DUDE_NATIVE_LOOK_TRIBAL,

    // Hero have finished Temple of Trails and received Vault Jumpsuit.
    DUDE_NATIVE_LOOK_JUMPSUIT,
    DUDE_NATIVE_LOOK_COUNT,
};

enum WeaponAnimation : int {
    WEAPON_ANIMATION_INVALID = -1,
    WEAPON_ANIMATION_NONE,
    WEAPON_ANIMATION_KNIFE, // d
    WEAPON_ANIMATION_CLUB, // e
    WEAPON_ANIMATION_HAMMER, // f
    WEAPON_ANIMATION_SPEAR, // g
    WEAPON_ANIMATION_PISTOL, // h
    WEAPON_ANIMATION_SMG, // i
    WEAPON_ANIMATION_SHOTGUN, // j
    WEAPON_ANIMATION_LASER_RIFLE, // k
    WEAPON_ANIMATION_MINIGUN, // l
    WEAPON_ANIMATION_LAUNCHER, // m
    WEAPON_ANIMATION_SFALL_S, // s
    WEAPON_ANIMATION_SFALL_O, // o
    WEAPON_ANIMATION_SFALL_P, // p
    WEAPON_ANIMATION_SFALL_Q, // q
    WEAPON_ANIMATION_SFALL_T, // t
    WEAPON_ANIMATION_COUNT,

    // There's mixed usage of WeaponAnimation and CharacterSoundEffect in the code, lets merge those as we any cannot distinguish between them.
    CHARACTER_SOUND_EFFECT_UNUSED = WEAPON_ANIMATION_NONE,
    CHARACTER_SOUND_EFFECT_KNOCKDOWN = WEAPON_ANIMATION_KNIFE,
    CHARACTER_SOUND_EFFECT_PASS_OUT = WEAPON_ANIMATION_CLUB,
    CHARACTER_SOUND_EFFECT_DIE = WEAPON_ANIMATION_HAMMER,
    CHARACTER_SOUND_EFFECT_CONTACT = WEAPON_ANIMATION_SPEAR,
};

inline bool weaponAnimationIsValid(int weaponAnimation)
{
    return weaponAnimation >= WEAPON_ANIMATION_NONE && weaponAnimation < WEAPON_ANIMATION_COUNT;
}

inline WeaponAnimation weaponAnimationFromFid(int fid)
{
    int anim = (fid & 0xF000) >> 12;
    return static_cast<WeaponAnimation>(anim);
}

enum class SkillDexFrameId : int {
    Invalid = -1, // invalid frame id
    Strength = 0, // strength.frm - strength (basic stat)
    Perception = 1, // perceptn.frm - perception (basic stat)
    Endurance = 2, // endur.frm - endurance (basic stat)
    Charisma = 3, // charisma.frm - charisma (basic stat)
    Intelligence = 4, // intel.frm - intelligence (basic stat)
    Agility = 5, // agility.frm - agility (basic stat)
    Luck = 6, // luck.frm - luck (basic stat)
    Level = 7, // level.frm - level
    Experience = 8, // exper.frm - experience
    NextLevel = 9, // levelnxt.frm - next level
    HitPoints = 10, // hitpoint.frm - hit points
    Poisoned = 11, // poisoned.frm - poisoned
    Radiated = 12, // radiated.frm - radiated
    EyeDamage = 13, // eyedamag.frm - eye damage
    CrippledRightArm = 14, // armright.frm - crippled right arm
    CrippledLeftArm = 15, // armleft.frm - crippled left arm
    CrippledRightLeg = 16, // legright.frm - crippled right leg
    CrippledLeftLeg = 17, // legleft.frm - crippled left leg
    ArmorClass = 18, // armorcls.frm - armor class
    ActionPoints = 19, // actionpt.frm - action points
    CarryWeight = 20, // carryamt.frm - carry weight (carry amount)
    MeleeDamage = 21, // meleedam.frm - melee damage
    DamageResistance = 22, // damresis.frm - damage resistance
    PoisonResistance = 23, // poisnres.frm - poison resistance
    Sequence = 24, // sequence.frm - sequence
    HealingRate = 25, // healrate.frm - healing rate
    CriticalChance = 26, // critchnc.frm - critical chance
    Skills = 27, // skills.frm - skills/tag skills/skill points
    SmallGuns = 28, // gunsml.frm - small guns
    BigGuns = 29, // gunbig.frm - big guns
    EnergyWeapons = 30, // energywp.frm - energy weapons
    Unarmed = 31, // unarmed.frm - unarmed
    MeleeWeapons = 32, // melee.frm - melee weapons
    Throwing = 33, // throwing.frm - throwing
    FirstAid = 34, // firstaid.frm - first aid
    Doctor = 35, // doctor.frm - doctor
    Sneak = 36, // sneak.frm - sneak
    Lockpick = 37, // lockpick.frm - lockpick
    Steal = 38, // steal.frm - steal
    Traps = 39, // traps.frm - traps
    Science = 40, // science.frm - science
    Repair = 41, // repair.frm - repair
    Speech = 42, // speech.frm - speech
    Barter = 43, // barter.frm - barter
    Gambling = 44, // gambling.frm - gambling
    Outdoorsman = 45, // outdoors.frm - outdoorsman
    Kills = 46, // kills.frm - kills
    Karma = 47, // karma.frm - karma
    Reputation = 48, // rep.frm - reputation (good/bad)
    AlcoholAddiction = 52, // alcohol.frm - addiction (alcohol)
    DrugAddiction = 53, // addict.frm - addiction (drugs)
    Traits = 54, // traits.frm - traits
    FastMetabolism = 55, // fastmeta.frm - fast metabolism
    Bruiser = 56, // bruiser.frm - bruiser
    SmallFrame = 57, // smlframe.frm - small frame
    OneHander = 58, // onehand.frm - one hander
    Finesse = 59, // finesse.frm - finesse
    Kamikaze = 60, // kamikaze.frm - kamikaze
    HeavyHanded = 61, // heavyhnd.frm - heavy handed
    FastShot = 62, // fastshot.frm - fast shot
    BloodyMess = 63, // bldmess.frm - bloody mess
    Jinxed = 64, // jinxed.frm - jinxed
    GoodNatured = 65, // goodnatr.frm - good natured
    DrugAddict = 66, // addict.frm - drug addict
    DrugResistant = 67, // drugrest.frm - drug resistant
    Skilled = 69, // skilled.frm - skilled
    Gifted = 70, // gifted.frm - gifted
    Perks = 71, // perks.frm - perks
    Awareness = 72, // awarenes.frm - awareness
    BonusHandToHandAttacks = 73, // hnd2hnd.frm - bonus hand-to-hand attacks
    BonusHandToHandDamage = 74, // damage.frm - bonus hand-to-hand damage
    BonusMove = 75, // bonusmve.frm - bonus move
    BonusRangedDamage = 76, // bonusrng.frm - bonus ranged damage
    BonusRateOfFire = 77, // bonusrat.frm - bonus rate of fire
    EarlierSequence = 78, // earlyseq.frm - earlier sequence
    FasterHealing = 79, // healrate.frm - faster healing
    MoreCriticals = 80, // morecrit.frm - more criticals
    NightVision = 81, // nightviz.frm - night vision
    Presence = 82, // presence.frm - presence
    RadResistance = 83, // radresis.frm - rad resistance
    Toughness = 84, // toughnes.frm - toughness
    PackAnimal = 85, // packanim.frm - pack animal
    Sharpshooter = 86, // sharpsht.frm - sharpshooter
    SilentRunning = 87, // silntrun.frm - silent running
    Survivalist = 88, // survival.frm - survivalist
    MasterTrader = 89, // mstrtrad.frm - master trader
    Educated = 90, // educated.frm - educated
    Healer = 91, // healer.frm - healer
    FortuneFinder = 92, // fortunfd.frm - fortune finder
    BetterCriticals = 93, // betrcrit.frm - better criticals
    Empathy = 94, // empathy.frm - empathy
    Slayer = 95, // slayer.frm - slayer
    Sniper = 96, // sniper.frm - sniper
    SilentDeath = 97, // silentd.frm - silent death
    ActionBoy = 98, // action.frm - action boy
    MentalBlock = 99, // mentalbk.frm - mental block
    Lifegiver = 100, // lifegivr.frm - lifegiver
    Dodger = 101, // dodger.frm - dodger
    Snakeater = 102, // snakeeat.frm - snakeater
    MrFixit = 103, // mrfixit.frm - mr fixit
    Medic = 104, // medic.frm - medic
    MasterThief = 105, // mtrthief.frm - master thief
    Speaker = 106, // speaker.frm - speaker
    HeaveHo = 107, // heaveho.frm - heave ho
    FriendlyFoe = 108, // frienfoe.frm - friendly foe
    Pickpocket = 109, // pickpock.frm - pickpocket
    Ghost = 110, // ghost.frm - ghost
    CultOfPersonality = 111, // cultoper.frm - cult of personality
    Scrounger = 112, // scroungr.frm - scrounger
    Explorer = 113, // explorer.frm - explorer
    FlowerChild = 114, // flower.frm - flower child
    Pathfinder = 115, // pathfndr.frm - pathfinder
    AnimalFriend = 116, // animalfr.frm - animal friend
    Scout = 117, // scout.frm - scout
    MysteriousStranger = 118, // stranger.frm - mysterious stranger
    Ranger = 119, // ranger.frm - ranger
    QuickPockets = 120, // quikpock.frm - quick pockets
    SmoothTalker = 121, // smoothtk.frm - smooth talker
    SwiftLearner = 122, // swftlern.frm - swift learner
    Tag = 123, // tag.frm - tag
    Mutate = 124, // mutate.frm - mutate
    Betrayer = 125, // betray.frm - betrayer
    BuffoutAddiction = 126, // buffouts.frm - buffout addiction
    DefenderReputation = 127, // defender.frm - defender rep
    DemonReputation = 128, // demon.frm - demon rep
    DespairReputation = 129, // despair.frm - despair rep
    Expert = 130, // expertsx.frm - expert (sex for new reno)
    FighterReputation = 131, // fighter.frm - fighter rep
    Gigolo = 132, // gigolo.frm - gigolo (sex for new reno)
    GraveRobber = 133, // graverob.frm - grave-robber
    GuardianReputation = 134, // guardian.frm - guardian rep
    IdolizedReputation = 135, // idolized.frm - idolized rep
    JetAddiction = 136, // jetadict.frm - jet addiction
    LikedReputation = 137, // liked.frm - liked rep
    MadeManReputation = 138, // mademan.frm - made man rep
    MarriedReputation = 139, // married.frm - married rep
    MentatsAddiction = 140, // mentats.frm - mentats addiction
    NeutralReputation = 141, // neutral.frm - neutral rep
    NukaColaAddiction = 142, // nukacola.frm - nuka cola addiction
    PsychoAddiction = 144, // psycho.frm - psycho addiction
    RadawayAddiction = 145, // radaway.frm - radaway addiction
    TragicCardGameAddiction = 149, // tragic.frm - tragic card game addiction
    VillifiedReputation = 150, // villfied.frm - villified rep
    HatedReputation = 153, // hated.frm - hated rep
    GenericVaultGuy = 154, // generic.frm - generic vault guy
    AdrenalineRush = 155, // adrnrush.frm - adrenaline rush
    CautiousNature = 156, // cautious.frm - cautious nature
    DermalArmor = 157, // drmlarmr.frm - dermal armor
    GeckoSkinning = 158, // geckoskn.frm - gecko skinning
    HandToHandEvade = 159, // h2hevade.frm - hand to hand evade
    Harmless = 160, // harmless.frm - harmless (changed name?)
    HereAndNow = 161, // here&now.frm - here and now
    KarmaBeacon = 162, // karmabcn.frm - karma beacon
    KamaSutraMaster = 163, // kmasutra.frm - kama sutra master
    LightStep = 164, // litestep.frm - light step
    LivingAnatomy = 165, // lvnganat.frm - living anatomy
    MagneticPersonality = 166, // magpers.frm - magnetic personality
    PackRat = 167, // packrat.frm - pack rat
    PhoenixArmor = 168, // phnxarmr.frm - phoenix armor
    Pyromaniac = 169, // pyromnac.frm - pyromaniac
    QuickRecovery = 170, // qwkrecov.frm - quick recovery
    Stonewall = 171, // stonwall.frm - stonewall
    VaultCityInoculations = 172, // vcinnoc.frm - vault city inoculations
    WeaponHandling = 173, // wepnhand.frm - weapon handling
};

enum CritterFrameId : int {
    CRITTER_FRM_ID_INVALID = -1,
    CRITTER_FRM_ID_FIRST = 0,
    CRITTER_FRM_ID_1 = 1,
};

inline constexpr CritterFrameId operator+(CritterFrameId lhs, int rhs)
{
    return static_cast<CritterFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr CritterFrameId operator-(CritterFrameId lhs, int rhs)
{
    return static_cast<CritterFrameId>(static_cast<int>(lhs) - rhs);
}

inline CritterFrameId operator++(CritterFrameId& e, int)
{
    CritterFrameId result = e;
    e = e + 1;
    return result;
}

inline CritterFrameId critterFrameIdFromFid(int fid)
{
    return static_cast<CritterFrameId>(frameIdFromFid(fid));
}

inline CritterFrameId critterFrameIdFromPid(int pid)
{
    return static_cast<CritterFrameId>(frameIdFromPid(pid));
}

enum SceneryFrameId : int {
    SCENERY_FRM_ID_FIRST = 0,
};

inline constexpr SceneryFrameId operator-(SceneryFrameId lhs, int rhs)
{
    return static_cast<SceneryFrameId>(static_cast<int>(lhs) - rhs);
}

inline SceneryFrameId sceneryFrameIdFromPid(int pid)
{
    return static_cast<SceneryFrameId>(frameIdFromPid(pid));
}

enum WallFrameId : int {
    WALL_FRM_ID_FIRST = 0,
};

inline constexpr WallFrameId operator-(WallFrameId lhs, int rhs)
{
    return static_cast<WallFrameId>(static_cast<int>(lhs) - rhs);
}

inline WallFrameId wallFrameIdFromPid(int pid)
{
    return static_cast<WallFrameId>(frameIdFromPid(pid));
}

enum ItemFrameId : int {
    ITEM_FRM_ID_FIRST = 0,
};

inline constexpr ItemFrameId operator-(ItemFrameId lhs, int rhs)
{
    return static_cast<ItemFrameId>(static_cast<int>(lhs) - rhs);
}

inline ItemFrameId itemFrameIdFromPid(int pid)
{
    return static_cast<ItemFrameId>(frameIdFromPid(pid));
}

enum TileFrameId : int {
    TILE_FRM_ID_FIRST = 0,
    TILE_FRM_ID_1 = 1,
    TILE_FRM_ID_LAST = 4095
};

inline constexpr TileFrameId operator+(TileFrameId lhs, int rhs)
{
    return static_cast<TileFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr TileFrameId operator-(TileFrameId lhs, int rhs)
{
    return static_cast<TileFrameId>(static_cast<int>(lhs) - rhs);
}

inline TileFrameId operator++(TileFrameId& e, int)
{
    TileFrameId result = e;
    e = e + 1;
    return result;
}

inline TileFrameId tileFrameIdFromFid(int fid)
{
    return static_cast<TileFrameId>(frameIdFromFid(fid));
}

inline TileFrameId tileFrameIdFromPid(int pid)
{
    return static_cast<TileFrameId>(frameIdFromPid(pid));
}

enum class MiscFrameId : int {
    Invalid = -1, // invalid frame id
    Reserved = 0, // reserved.frm
    EmpExplosion = 2, // empxpld.frm
    RocketExplosion = 10, // roktxpd.frm
    ScrollBlocker = 12, // scrblk.frm
    FireExplosion = 29, // expa.frm
    PlasmaExplosion = 31, // expp.frm
    Exit2Grid1 = 33, // ext2grd1.frm
    Exit3Grid8 = 48, // ext3grd8.frm
};

enum class InterfaceFrameId : int {
    Invalid = -1, // invalid frame id
    First = 0, // first frame id in interface.lst
    Last = 4095, // last possible frame id in interface.lst
    Blank = 0, // blank.frm - used be mset000.frm for top of bouncing mouse cursor
    HexMouseCursor = 1, // msef000.frm - hex mouse cursor
    Egg = 2, // egg.frm - used for the translucent "egg" effect around player
    ExitGridMarker = 3, // msef001.frm - exit grid marker
    BigRedButtonUp = 6, // bigredup.frm - big red button up
    BigRedButtonDown = 7, // bigreddn.frm - big red button down
    LittleRedButtonUp = 8, // lilredup.frm - little red button up
    LittleRedButtonDown = 9, // lilreddn.frm - little red button down
    AutomapButtonDown = 10, // mapdn.frm - automap button down
    AutomapButtonUp = 13, // mapup.frm - automap button up
    MainInterface = 16, // iface.frm - main interface
    OptionsButtonDown = 17, // optidn.frm - options button down
    OptionsButtonUp = 18, // optiup.frm - options button up
    SingleAttackBigDown = 31, // sattkbdn.frm - single attack big down
    SingleAttackBigUp = 32, // sattkbup.frm - single attack big up
    CharacterEditorTraitsFolder = 38, // karmafdr.frm - Character editor  * Place holder for traits folder image *
    BurstText = 40, // burst.frm - burst text
    KickText = 41, // kick.frm - kick text
    PunchText = 42, // punch.frm - punch text
    SingleText = 43, // single.frm - single text
    SwingText = 44, // swing.frm - swing text
    ThrustText = 45, // thrust.frm - thrust text
    InventoryButtonDown = 46, // invbutdn.frm - inventory button down
    InventoryButtonUp = 47, // invbutup.frm - inventory button up
    InventoryBox = 48, // invbox.frm - inventory box
    InventoryButtonOutUp = 49, // invupout.frm - inventory button out up
    InventoryButtonInUp = 50, // invupin.frm - inventory button in up
    InventoryButtonOutDown = 51, // invdnout.frm - inventory button out down
    InventoryButtonInDown = 52, // invdnin.frm - inventory button in down
    InventoryButtonUpDisabled = 53, // invupds.frm - inventory button up disabled
    InventoryButtonDownDisabled = 54, // invdnds.frm - inventory button down disabled
    CharacterButtonDown = 56, // chadn.frm - character button down
    CharacterButtonUp = 57, // chaup.frm - character button up
    PipBoyButtonDown = 58, // pipdn.frm - pipboy button down
    PipBoyButtonUp = 59, // pipup.frm - pipboy button up
    SingleAttackBigUpUnreadied = 73, // saturbup.frm - single attack big up unreadied
    HitPointsNumbers = 82, // numbers.frm - numbers for the hit points and fatigue counters
    HealthGreenLight = 83, // hlgrn.frm - green health light
    HealthYellowLight = 84, // hlyel.frm - yellow health light
    HealthRedLight = 85, // hlred.frm - red health light
    PerkDialogBackground = 86, // perkwin.frm - perk dialog background
    DialogBigDownArrowUp = 87, // di_bgdn1.frm - dialog big down arrow <UP>
    DialogBigDownArrowDown = 88, // di_bgdn2.frm - dialog big down arrow <DOWN>
    DialogBigUpArrowUp = 89, // di_bgup1.frm - dialog big up arrow <UP>
    DialogBigUpArrowDown = 90, // di_bgup2.frm - dialog big up arrow <DOWN>
    DialogBigDoneButtonUp = 91, // di_done1.frm - dialog big done button <UP>
    DialogBigDoneButtonDown = 92, // di_done2.frm - dialog big done button <DOWN>
    DialogDownButtonUp = 93, // di_down1.frm - dialog down button <UP>
    DialogDownButtonDown = 94, // di_down2.frm - dialog down button <DOWN>
    DialogRedButtonDown = 95, // di_rdbt1.frm - dialog red button <DOWN>
    DialogRedButtonUp = 96, // di_rdbt2.frm - dialog red button <UP>
    DialogRestButtonUp = 97, // di_rest1.frm - dialog rest button <UP>
    DialogRestButtonDown = 98, // di_rest2.frm - dialog rest button <DOWN>
    DialogTalkSubwindow = 99, // di_talk.frm - dialog screen subwindow (NPC's)
    DialogUpButtonUp = 100, // di_up1.frm - dialog up button <UP>
    DialogUpButtonDown = 101, // di_up2.frm - dialog up button <DOWN>
    DialogReviewBackground = 102, // review.frm - review screen
    DialogScreenBackground = 103, // alltlk.frm - dialog screen background
    EndTurnWindowAnimation = 104, // endanim.frm - endturn window open/close animation
    EndTurnButtonUp = 105, // endturnu.frm - end turn up image
    EndTurnButtonDown = 106, // endturnd.frm - end turn down image
    EndCombatButtonUp = 107, // endcmbtu.frm - end combat up image
    EndCombatButtonDown = 108, // endcmbtd.frm - end combat down image
    EndTurnGreenLight = 109, // endltgrn.frm - green lights around end turn/combat window
    EndTurnRedLight = 110, // endltred.frm - red lights around end turn/combat window
    BarterWindow = 111, // barter.frm - barter window
    UseItemOnWindow = 113, // use.frm - use_item_on window
    LootContainerWindow = 114, // loot.frm - loot_container window
    DialogueUpperHighlight = 115, // hilight1.frm - dialogue upper hilight
    DialogueLowerHighlight = 116, // hilight2.frm - dialogue lower hilight
    ThrowText = 117, // throw.frm - throw text
    CalledShotWindow = 118, // called.frm - called shot window
    SkilldexOnButton = 119, // skldxon.frm - Skilldex on button
    SkilldexOffButton = 120, // skldxoff.frm - Skilldex off button
    SkilldexWindow = 121, // skldxbox.frm - Skilldex window
    LeftArrowUp = 122, // slu.frm - Left arrow up
    LeftArrowDown = 123, // sld.frm - Left arrow down
    RightArrowUp = 124, // sru.frm - Right arrow up
    RightArrowDown = 125, // srd.frm - Right arrow down
    WarningBox = 126, // warnbox.frm - Interconnection warning window box
    PipBoyWindow = 127, // pip.frm - pip boy window
    PipBoyNoteAboutVats = 128, // pip2.frm - pip boy note about the vats
    PipBoyMonthStrings = 129, // months.frm - month strings for pip boy
    PipBoyNoteNumbers = 130, // notenums.frm - pip boy note numbers
    PipBoySleepAlarmIn = 131, // alarmin.frm - pip boy sleep alarm - in
    PipBoySleepAlarmOut = 132, // alarmout.frm - pip boy sleep alarm - out
    PipBoyLogo = 133, // pipx.frm - pipboy 2000 logo
    WorldMapDialogBox = 136, // wmapbox.frm - World map dialog box
    WorldMapLocationMarker = 138, // wmaploc.frm - World map location marker
    WorldMapMoveTargetMarker1 = 139, // wmaptarg.frm - World map move target maker #1
    MainMenuBackgroundImage = 140, // mainmenu.frm - main menu background image
    MapElevatorButtonIn = 141, // ebut_in.frm - Map elevator screen
    MapElevatorButtonOut = 142, // ebut_out.frm - Map elevator screen
    MapElevatorBos = 143, // el_bos.frm - Map elevator screen
    MapElevatorMast1 = 144, // el_mast1.frm - Map elevator screen
    MapElevatorMast2 = 145, // el_mast2.frm - Map elevator screen
    MapElevatorMil1 = 146, // el_mil1.frm - Map elevator screen
    MapElevatorMil2 = 147, // el_mil2.frm - Map elevator screen
    MapElevatorVault = 148, // el_vault.frm - Map elevator screen
    MapElevatorGaj000 = 149, // gaj000.frm - Map elevator screen
    MapElevatorBos2 = 150, // el_bos2.frm - Map elevator screen
    MapElevatorMil3 = 151, // el_mil3.frm - Map elevator screen
    WorldMapFightIcon1 = 154, // wmapfgt0.frm - world map fight icon #1
    WorldMapFightIcon2 = 155, // wmapfgt1.frm - world map fight icon #2
    TownMapHotspot1 = 168, // hotspot1.frm - town map selector shape #1
    CharacterEditorCreateBackground = 169, // edtrcrte.frm - character editor background screen #1
    BigNum = 170, // bignum.frm - character editor
    AutomapWindow = 171, // automap.frm - automap window
    AutoUp = 172, // autoup.frm - switch up
    AutoDown = 173, // autodwn.frm - switch down
    CharacterSelectorBackground = 174, // pickchar.frm - character selector background
    CharacterEditorAgeMask = 175, // agemask.frm - Character editor
    CharacterEditorAgeOff = 176, // ageoff.frm - Character editor
    CharacterEditorEditBackground = 177, // edtredt.frm - Character editor background screen #2
    CharacterEditorKarmaFolder = 178, // karmafdr.frm - Character editor
    CharacterEditorKillsFolder = 179, // killsfdr.frm - Character editor
    CharacterEditorPerksFolder = 180, // perksfdr.frm - Character editor
    CharacterEditorDownArrowOff = 181, // dnarwoff.frm - character editor
    CharacterEditorDownArrowOn = 182, // dnarwon.frm - character editor
    CharacterEditorNameMask = 183, // namemsk.frm - Character editor
    CharacterEditorNameOn = 184, // nameon.frm - Character editor
    CharacterEditorNameOff = 185, // nameoff.frm - Character editor
    CharacterEditorFolderMask = 186, // fldrmask.frm - Character editor
    CharacterEditorSexMask = 187, // sexmask.frm - Character editor
    CharacterEditorSexOff = 188, // sexoff.frm - Character editor
    CharacterEditorSexOn = 189, // sexon.frm - Character editor
    CharacterEditorSlider = 190, // slider.frm - Character editor
    CharacterEditorSpecialNegativeOff = 191, // snegoff.frm - Character editor
    CharacterEditorSpecialNegativeOn = 192, // snegon.frm - Character editor
    CharacterEditorSpecialPositiveOff = 193, // splsoff.frm - Character editor
    CharacterEditorSpecialPositiveOn = 194, // splson.frm - Character editor
    CharacterEditorStrengthNegativeOff = 195, // stnegoff.frm - Character editor
    CharacterEditorStrengthNegativeOn = 196, // stnegon.frm - Character editor
    CharacterEditorStrengthPositiveOff = 197, // stplsoff.frm - Character editor
    CharacterEditorStrengthPositiveOn = 198, // stplson.frm - Character editor
    CharacterEditorUpArrowOff = 199, // uparwoff.frm - character editor
    CharacterEditorUpArrowOn = 200, // uparwon.frm - character editor
    PremadeCharacterCombat = 201, // combat.frm - Premade character portrait
    PremadeCharacterStealth = 202, // stealth.frm - Premade character portrait
    PremadeCharacterDiplomat = 203, // diplomat.frm - Premade character portrait
    CharacterEditorAgeOn = 204, // ageon.frm - Character editor
    CharacterEditorAgeBox = 205, // agebox.frm - Character editor
    CharacterEditorAttributesBox = 206, // attribox.frm - Character editor
    CharacterEditorAttributesWindow = 207, // attribwn.frm - Character editor
    CharacterWindow = 208, // charwin.frm - character editor
    DoneBox = 209, // donebox.frm - character editor
    CharacterEditorFemaleOff = 210, // femoff.frm - Character editor
    CharacterEditorFemaleOn = 211, // femon.frm - Character editor
    CharacterEditorMaleOff = 212, // maleoff.frm - Character editor
    CharacterEditorMaleOn = 213, // maleon.frm - Character editor
    CharacterEditorNameBox = 214, // namebox.frm - Character editor
    CharacterEditorTaggedSkillsOff = 215, // tgskloff.frm - Character editor
    CharacterEditorTaggedSkillsOn = 216, // tgsklon.frm - Character editor
    LargeDialog = 217, // lgdialog.frm - Large generic dialog box
    MediumDialog = 218, // medialog.frm - Medium generic dialog box
    CharacterEditorBarArrows = 219, // bararrws.frm - Character editor
    OptionsBase = 220, // opbase.frm - character editor
    OptionsButtonOff = 221, // opbtnoff.frm - character editor
    OptionsButtonOn = 222, // opbtnon.frm - character editor
    TownMapHotspot2 = 223, // hotspot2.frm - town map selector shape #2
    LoadBox = 224, // loadbox.frm - character editor
    SaveBox = 225, // savebox.frm - character editor
    PipBoyBomb = 226, // bomb1.frm - Pipboy
    LoadSaveGame = 237, // lsgame.frm - load/save game
    LoadSaveBox = 238, // lsgbox.frm - load/save game
    LoadSaveCover = 239, // lscover.frm - load/save game
    PreferencesScreen = 240, // prefscrn.frm - options screen
    PreferencesSliderOff = 241, // prfsldof.frm - options screen
    PreferencesBackground = 242, // prfbknbs.frm - options screen
    PreferencesBackgroundLeft = 243, // prflknbs.frm - options screen
    PreferencesXIn = 244, // prfxin.frm - options screen
    PreferencesXOut = 245, // prfxout.frm - options screen
    PreferencesCover = 246, // prefcvr.frm - options screen
    PreferencesSliderOn = 247, // prfsldon.frm - options screen
    ActionMove = 249, // msef003.frm - Action move
    ActionArrow = 250, // actarrow.frm - Action arrow
    ActionCrosshair = 251, // acrshair.frm - Action crosshair
    CancelHighlighted = 252, // cancelh.frm - Action menu cancel highlighted
    Cancel = 253, // canceln.frm - Action menu cancel normal
    DropHighlighted = 254, // droph.frm - Action menu drop highlighted
    Drop = 255, // dropn.frm - Action menu drop normal
    InventoryHighlighted = 256, // invenh.frm - Action menu inventory highlighted
    Inventory = 257, // invenn.frm - Action menu inventory normal
    LookHighlighted = 258, // lookh.frm - Action menu look highlighted
    Look = 259, // lookn.frm - Action menu look normal
    RotateHighlighted = 260, // rotateh.frm - Action menu rotate highlighted
    Rotate = 261, // rotaten.frm - Action menu rotate normal
    TalkHighlighted = 262, // talkh.frm - Action menu talk highlighted
    Talk = 263, // talkn.frm - Action menu talk normal
    UseGetHighlighted = 264, // usegeth.frm - Action menu use/get highlighted
    UseGet = 265, // usegetn.frm - Action menu use/get normal
    MouseCursorBlank = 266, // blank.frm - Mouse cursor - none ( 1 x 1 transparent)
    MouseCursorStandardArrow = 267, // stdarrow.frm - Mouse cursor - standard arrow
    MouseCursorSmallUpArrow = 268, // suparrow.frm - Mouse cursor - small up arrow
    MouseCursorSmallDownArrow = 269, // sdnarrow.frm - Mouse cursor - small down arrow
    MouseCursorScrollNorthWest = 270, // scrnwest.frm - Mouse cursor - scroll northwest
    MouseCursorScrollNorth = 271, // scrnorth.frm - Mouse cursor - scroll north
    MouseCursorScrollNorthEast = 272, // scrneast.frm - Mouse cursor - scroll northeast
    MouseCursorScrollEast = 273, // screast.frm - Mouse cursor - scroll east
    MouseCursorScrollSouthEast = 274, // scrseast.frm - Mouse cursor - scroll southeast
    MouseCursorScrollSouth = 275, // scrsouth.frm - Mouse cursor - scroll south
    MouseCursorScrollSouthWest = 276, // scrswest.frm - Mouse cursor - scroll southwest
    MouseCursorScrollWest = 277, // scrwest.frm - Mouse cursor - scroll west
    MouseCursorWaitPlanet = 278, // wait.frm - Mouse cursor - wait (planet)
    MouseCursorCrosshair = 279, // crsshair.frm - Mouse cursor - crosshair
    MouseCursorPlus = 280, // plus.frm - Mouse cursor - plus
    MouseCursorDestroy = 281, // destroy.frm - Mouse cursor - destroy/erase
    ActionPick = 282, // actpick.frm - action pick
    ActionMenu = 283, // actmenu.frm - action menu
    ActionToHit = 284, // acttohit.frm - action to hit
    ActionArrowMirrored = 285, // actarrom.frm - Action arrow (mirrored)
    PointingHand = 286, // hand.frm - pointing hand used in the inventory window
    Bullseye = 288, // bullseye.frm - bullseye for interface button
    MovementPointText = 289, // mvepnt.frm - movement point text
    MovementPointsNumbers = 290, // mvenum.frm - movement point numbers
    ReloadText = 291, // reload.frm - reload text
    UseText = 292, // uset.frm - use text
    MouseCursorUseCrosshair = 293, // crossuse.frm - Mouse cursor - use crosshair
    UseOnText = 294, // useon.frm - use on text
    MouseCursorWaitWatch = 295, // wait2.frm - Mouse cursor - wait (watch)
    HelpBackground = 297, // helpscrn.frm - Single frame online help screen
    MainMenuButtonUp = 299, // menuup.frm - Up button for main menu
    MainMenuButtonDown = 300, // menudown.frm - Down button for main menu
    UnloadHighlighted = 301, // unloadh.frm - Action menu unload highlighted
    Unload = 302, // unloadn.frm - Action menu unload normal
    SkillHighlighted = 303, // skillh.frm - Action menu skill highlighted
    Skill = 304, // skilln.frm - Action menu skill normal
    MoveMultipleItemsInterface = 305, // movemult.frm - move multiple items interface
    TimerOverlay = 306, // timer.frm - timer overlay for move multiple items interface
    AllButtonPressed = 307, // allbon.frm - ALL button (pressed) for move multiple items interface
    AllButtonUnpressed = 308, // allboff.frm - ALL button (unpressed) for move multiple items interface
    DeathScene = 309, // death.frm - Dead in the wasteland scene
    MouseCursorSingleFrameWatch = 310, // watch.frm - Single-frame watch cursor
    PanningDesertImage = 327, // dp.frm - panning desert image
    MouseCursorInvalidScrollEast = 328, // screx.frm - Mouse cursor - invalid scroll east
    MouseCursorInvalidScrollNorthEast = 329, // scrnex.frm - Mouse cursor - invalid scroll northeast
    MouseCursorInvalidScrollNorthWest = 330, // scrnwx.frm - Mouse cursor - invalid scroll northwest
    MouseCursorInvalidScrollNorth = 331, // scrnx.frm - Mouse cursor - invalid scroll north
    MouseCursorInvalidScrollSouthEast = 332, // scrsex.frm - Mouse cursor - invalid scroll southeast
    MouseCursorInvalidScrollSouthWest = 333, // scrswx.frm - Mouse cursor - invalid scroll southwest
    MouseCursorInvalidScrollSouth = 334, // scrsx.frm - Mouse cursor - invalid scroll south
    MouseCursorInvalidScrollWest = 335, // scrwx.frm - Mouse cursor - invalid scroll west
    WorldSphereOverlay0 = 336, // wrldspr0.frm - World Sphere Overlay 0
    WorldSphereOverlay1 = 337, // wrldspr1.frm - World Sphere Overlay 1
    WorldSphereOverlay2 = 338, // wrldspr2.frm - World Sphere Overlay 2
    WorldMapOverlayScreen = 363, // wmscreen.frm - Worldmap Overlay Screen
    WorldMapTownTabsUnderlay = 364, // wmtabs.frm - worldmap town tabs underlay
    WorldMapNightDayDial = 365, // wmdial.frm - worldmap night/day dial
    WorldMapGlobeStampOverlay = 366, // wmglobe.frm - worldmap globe stamp overlay
    WorldMapTownTabsEdgingOverlay = 367, // wmtbedge.frm - worldmap town tabs edging overlay
    MapElevatorSierraBase = 388, // el_base1.frm - Map elevator screen for sierra base
    DialogTalkSubwindowParty = 389, // di_talkp.frm - dialog screen subwindow (party members)
    PartyControlInterface = 390, // control.frm - party member control interface
    PartyCustomInterface = 391, // custom.frm - party member control interface
    PartyAggressiveDown = 392, // aggdn.frm - party member control interface (aggressive down)
    PartyAggressiveDisabled = 393, // aggoff.frm - party member control interface (aggressive disabled)
    PartyAggressiveUp = 394, // aggup.frm - party member control interface (aggressive up)
    PartyBerserkDown = 395, // berdn.frm - party member control interface (berserk down)
    PartyBerserkDisabled = 396, // beroff.frm - party member control interface (berserk disabled)
    PartyBerserkUp = 397, // berup.frm - party member control interface (berserk up)
    PartyCowardDown = 398, // cowdn.frm - party member control interface (coward down)
    PartyCowardDisabled = 399, // cowoff.frm - party member control interface (coward disabled)
    PartyCowardUp = 400, // cowup.frm - party member control interface (coward up)
    PartyCustomDown = 401, // cusdn.frm - party member control interface (custom down)
    PartyCustomDisabled = 402, // cusoff.frm - party member control interface (custom disabled)
    PartyCustomUp = 403, // cusup.frm - party member control interface (custom up)
    PartyDefensiveDown = 404, // defdn.frm - party member control interface (defensive down)
    PartyDefensiveDisabled = 405, // defoff.frm - party member control interface (defensive disabled)
    PartyDefensiveUp = 406, // defup.frm - party member control interface (defensive up)
    PartyAttackDown = 407, // attackdn.frm - party member custom interface
    PartyAttackUp = 408, // attackup.frm - party member custom interface
    PartyBurstDown = 409, // burstdn.frm - party member custom interface
    PartyBurstUp = 410, // burstup.frm - party member custom interface
    PartyChemDown = 411, // chemdn.frm - party member custom interface
    PartyChemUp = 412, // chemup.frm - party member custom interface
    PartyDistanceDown = 413, // distdn.frm - party member custom interface
    PartyDistanceUp = 414, // distup.frm - party member custom interface
    PartyRunDown = 415, // rundn.frm - party member custom interface
    PartyRunUp = 416, // runup.frm - party member custom interface
    PartyWeaponDown = 417, // weapdn.frm - party member custom interface
    PartyWeaponUp = 418, // weapup.frm - party member custom interface
    PartyCustomSelect = 419, // cussel.frm - party member custom interface
    TradeWindow = 420, // trade.frm - party member barter/trade interface
    ChopPunch = 421, // cm_jab.frm - chopuch.frm ; chop punch text
    DeathBlossomKick = 422, // cm_prckk.frm - dblossk.frm ; death blossom kick text
    DragonPunch = 423, // cm_plmst.frm - dragpuch.frm ; dragon punch text
    ForcePunch = 424, // cm_pstrk.frm - forcpuch.frm ; force punch text
    HammerPunch = 425, // hampnch.frm - hammer punch text
    HipKick = 426, // hipk.frm - hip kick text
    JumpKick = 427, // cm_hookk.frm - jumpk.frm ; jump kick text
    LightningPunch = 428, // cm_hymkr.frm - lignpuch.frm ; lightning punch text
    RoundhouseKick = 429, // cm_pwkck.frm - roundk.frm ; roundhouse kick text
    StrongKick = 430, // skick.frm - strong kick text
    SnapKick = 431, // snapkick.frm - snap kick text
    StrongPunch = 432, // spunch.frm - strong punch text
    WorldMapCarMovie = 433, // wmcarmve.frm - WorldMap Car Movie
    PushHighlighted = 434, // pushh.frm - Action menu push highlighted
    Push = 435, // pushn.frm - Action menu push normal
    InventoryLootAllUp = 436, // invmaup.frm - Inventory Loot All
    InventoryLootAllDown = 437, // invmadn.frm - Inventory Loot All
    WorldMapRandomEncounterCursor2Bright = 438, // wmrnden2.frm - WorldMap Random Encounter Cursor #2 bright
    WorldMapRandomEncounterCursor2Dark = 439, // wmrnden3.frm - WorldMap Random Encounter Cursor #2 dark
};

} // namespace fallout
#endif /* ART_DEFS_H */
