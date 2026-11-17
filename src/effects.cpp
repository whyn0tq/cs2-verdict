#include "effects.h"
#include "clients.h"
#include "entities.h"
#include "settings.h"
#include "common.h"
#include "entity2/entitysystem.h"
#include "entity2/entitykeyvalues.h"

extern CEntitySystem* g_pEntitySystem;

static const float ExplosionLifetime = 3.0f;

static CEntityHandle g_FireEffects[MAX_SLOTS];

static CEntityInstance* Resolve(const CEntityHandle& handle)
{
    if (!handle.IsValid() || !g_pEntitySystem)
        return nullptr;
    return g_pEntitySystem->GetEntityInstance(handle);
}

static void RemoveByHandle(const CEntityHandle& handle)
{
    CEntityInstance* entity = Resolve(handle);
    if (entity)
        g_pUtils->RemoveEntity(entity);
}

static std::vector<int> Listeners()
{
    std::vector<int> listeners;
    for (int slot = 0; slot < MAX_SLOTS; slot++)
    {
        if (IsRealClient(slot))
            listeners.push_back(slot);
    }
    return listeners;
}

void PlaySoundAt(int slot, const std::string& sound)
{
    CEntityInstance* pawn = PawnOf(slot);
    if (!pawn || sound.empty())
        return;
    g_pPlayers->EmitSound(Listeners(), pawn->GetEntityIndex(), sound, 100, 1.0f);
}

static CEntityInstance* SpawnParticle(const std::string& effect, const float origin[3])
{
    if (effect.empty())
        return nullptr;

    CEntityInstance* particle = (CEntityInstance*)g_pUtils->CreateEntityByName("info_particle_system", CEntityIndex(-1));
    if (!particle)
        return nullptr;

    CEntityKeyValues* values = new CEntityKeyValues();
    values->SetString("effect_name", effect.c_str());
    values->SetBool("start_active", true);
    values->SetVector("origin", Vector(origin[0], origin[1], origin[2]));
    g_pUtils->DispatchSpawn(particle, values);
    return particle;
}

void StartFireEffect(int slot)
{
    if (!IsValidSlot(slot))
        return;
    StopFireEffect(slot);

    CEntityInstance* pawn = PawnOf(slot);
    float origin[3];
    if (!pawn || !OriginOf(slot, origin))
        return;

    CEntityInstance* particle = SpawnParticle(g_Settings.effects.fireParticle, origin);
    if (!particle)
        return;

    g_pUtils->AcceptEntityInput(particle, "SetParent", variant_t("!activator"), pawn, particle);
    g_FireEffects[slot] = particle->GetRefEHandle();
}

void StopFireEffect(int slot)
{
    if (!IsValidSlot(slot))
        return;
    RemoveByHandle(g_FireEffects[slot]);
    g_FireEffects[slot] = CEntityHandle();
}

void ShowExplosion(int slot)
{
    float origin[3];
    if (!OriginOf(slot, origin))
        return;

    CEntityInstance* particle = SpawnParticle(g_Settings.effects.explosionParticle, origin);
    if (!particle)
        return;

    CEntityHandle handle = particle->GetRefEHandle();
    g_pUtils->CreateTimer(ExplosionLifetime, [handle]() {
        RemoveByHandle(handle);
        return -1.0f;
    });
}

void ClearAllEffects()
{
    for (int slot = 0; slot < MAX_SLOTS; slot++)
        StopFireEffect(slot);
}
