#include "entities.h"
#include "common.h"
#include "schemasystem/schemasystem.h"
#include "entity2/entitysystem.h"

extern CEntitySystem* g_pEntitySystem;

#ifdef _WIN32
static const char* ServerModule = "server.dll";
#else
static const char* ServerModule = "libserver.so";
#endif

struct FieldInfo
{
    int offset;
    std::string owner;
};

static std::map<std::string, FieldInfo> g_Fields;

static bool SearchClass(SchemaClassInfoData_t* info, const char* fieldName, int baseOffset, FieldInfo& found)
{
    if (!info)
        return false;

    for (int i = 0; i < info->m_nFieldCount; i++)
    {
        if (strcmp(info->m_pFields[i].m_pszName, fieldName) == 0)
        {
            found.offset = baseOffset + info->m_pFields[i].m_nSingleInheritanceOffset;
            found.owner = info->m_pszName;
            return true;
        }
    }

    for (int i = 0; i < info->m_nBaseClassCount; i++)
    {
        SchemaBaseClassInfoData_t& base = info->m_pBaseClasses[i];
        if (SearchClass(base.m_pClass, fieldName, baseOffset + (int)base.m_nOffset, found))
            return true;
    }
    return false;
}

static FieldInfo FindField(const char* className, const char* fieldName)
{
    std::string key = std::string(className) + "::" + fieldName;
    std::map<std::string, FieldInfo>::iterator cached = g_Fields.find(key);
    if (cached != g_Fields.end())
        return cached->second;

    FieldInfo field;
    field.offset = -1;

    CSchemaSystemTypeScope* scope = g_pSchemaSystem->FindTypeScopeForModule(ServerModule);
    SchemaClassInfoData_t* info = scope ? scope->FindDeclaredClass(className).Get() : nullptr;
    if (!SearchClass(info, fieldName, 0, field))
    {
        field.offset = -1;
        g_pUtils->ErrorLog("[Verdict] Schema field %s not found", key.c_str());
    }

    g_Fields[key] = field;
    return field;
}

static char* FieldAddress(void* object, const char* className, const char* fieldName)
{
    if (!object)
        return nullptr;
    FieldInfo field = FindField(className, fieldName);
    if (field.offset < 0)
        return nullptr;
    return (char*)object + field.offset;
}

static void MarkChanged(CEntityInstance* entity, const char* className, const char* fieldName)
{
    FieldInfo field = FindField(className, fieldName);
    if (entity && field.offset >= 0)
        g_pUtils->SetStateChanged((CBaseEntity*)entity, field.owner.c_str(), fieldName);
}

static void WritePawnFloat(int slot, const char* fieldName, float value)
{
    CEntityInstance* pawn = PawnOf(slot);
    char* field = FieldAddress(pawn, "CCSPlayerPawn", fieldName);
    if (!field)
        return;
    *(float*)field = value;
    MarkChanged(pawn, "CCSPlayerPawn", fieldName);
}

static void WritePawnInt(int slot, const char* fieldName, int value)
{
    CEntityInstance* pawn = PawnOf(slot);
    char* field = FieldAddress(pawn, "CCSPlayerPawn", fieldName);
    if (!field)
        return;
    *(int*)field = value;
    MarkChanged(pawn, "CCSPlayerPawn", fieldName);
}

CEntityInstance* ControllerOf(int slot)
{
    if (!IsValidSlot(slot) || !g_pEntitySystem)
        return nullptr;
    return g_pEntitySystem->GetEntityInstance(CEntityIndex(slot + 1));
}

CEntityInstance* PawnOf(int slot)
{
    char* field = FieldAddress(ControllerOf(slot), "CCSPlayerController", "m_hPlayerPawn");
    if (!field)
        return nullptr;
    CEntityHandle handle = *(CEntityHandle*)field;
    if (!handle.IsValid())
        return nullptr;
    return g_pEntitySystem->GetEntityInstance(handle);
}

bool IsAlive(int slot)
{
    char* field = FieldAddress(ControllerOf(slot), "CCSPlayerController", "m_bPawnIsAlive");
    return field && *(bool*)field;
}

int TeamOf(int slot)
{
    char* field = FieldAddress(ControllerOf(slot), "CCSPlayerController", "m_iTeamNum");
    return field ? *(uint8*)field : TEAM_NONE;
}

int MoveTypeOf(int slot)
{
    char* field = FieldAddress(PawnOf(slot), "CCSPlayerPawn", "m_MoveType");
    return field ? *(uint8*)field : MOVETYPE_WALK;
}

int HealthOf(int slot)
{
    char* field = FieldAddress(PawnOf(slot), "CCSPlayerPawn", "m_iHealth");
    return field ? *(int*)field : 0;
}

void SetHealth(int slot, int health)
{
    WritePawnInt(slot, "m_iHealth", health);
}

void SetArmor(int slot, int armor)
{
    WritePawnInt(slot, "m_ArmorValue", armor);
}

void SetGravity(int slot, float scale)
{
    WritePawnFloat(slot, "m_flGravityScale", scale);

    char* actualScale = FieldAddress(PawnOf(slot), "CCSPlayerPawn", "m_flActualGravityScale");
    if (actualScale)
        *(float*)actualScale = scale;
}

void SetSpeed(int slot, float multiplier)
{
    WritePawnFloat(slot, "m_flVelocityModifier", multiplier);
}

bool HasGodMode(int slot)
{
    char* field = FieldAddress(PawnOf(slot), "CCSPlayerPawn", "m_bTakesDamage");
    return field && !*(bool*)field;
}

void SetGodMode(int slot, bool enabled)
{
    CEntityInstance* pawn = PawnOf(slot);
    char* field = FieldAddress(pawn, "CCSPlayerPawn", "m_bTakesDamage");
    if (!field)
        return;
    *(bool*)field = !enabled;
    MarkChanged(pawn, "CCSPlayerPawn", "m_bTakesDamage");
}

bool OriginOf(int slot, float origin[3])
{
    char* bodyField = FieldAddress(PawnOf(slot), "CCSPlayerPawn", "m_CBodyComponent");
    if (!bodyField)
        return false;

    void* body = *(void**)bodyField;
    char* nodeField = FieldAddress(body, "CBodyComponent", "m_pSceneNode");
    if (!nodeField)
        return false;

    void* node = *(void**)nodeField;
    char* originField = FieldAddress(node, "CGameSceneNode", "m_vecAbsOrigin");
    if (!originField)
        return false;

    float* values = (float*)originField;
    origin[0] = values[0];
    origin[1] = values[1];
    origin[2] = values[2];
    return true;
}

void WriteEntityColor(CEntityInstance* entity, int red, int green, int blue, int alpha)
{
    char* field = FieldAddress(entity, "CBaseModelEntity", "m_clrRender");
    if (!field)
        return;
    uint8* color = (uint8*)field;
    color[0] = (uint8)red;
    color[1] = (uint8)green;
    color[2] = (uint8)blue;
    color[3] = (uint8)alpha;
    MarkChanged(entity, "CBaseModelEntity", "m_clrRender");
}

void SetPlayerColor(int slot, int red, int green, int blue)
{
    WriteEntityColor(PawnOf(slot), red, green, blue, 255);
}

int ControllerIntOf(int slot, const char* fieldName)
{
    char* field = FieldAddress(ControllerOf(slot), "CCSPlayerController", fieldName);
    return field ? *(int*)field : 0;
}

int MatchStatOf(int slot, const char* fieldName)
{
    char* servicesField = FieldAddress(ControllerOf(slot), "CCSPlayerController", "m_pActionTrackingServices");
    if (!servicesField)
        return 0;
    void* services = *(void**)servicesField;
    char* stats = FieldAddress(services, "CCSPlayerController_ActionTrackingServices", "m_matchStats");
    if (!stats)
        return 0;
    char* field = FieldAddress(stats, "CSMatchStats_t", fieldName);
    return field ? *(int*)field : 0;
}

int EntityTeam(CEntityInstance* entity)
{
    char* field = FieldAddress(entity, "CBaseEntity", "m_iTeamNum");
    return field ? *(uint8*)field : TEAM_NONE;
}

int TeamScoreOf(CEntityInstance* team)
{
    char* field = FieldAddress(team, "CCSTeam", "m_iScore");
    return field ? *(int*)field : 0;
}
