#include "PrecisionAPI.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

using namespace RE;
using namespace SKSE;

namespace WeaponTip {

    // Configuração de log
    void InitLogging()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= "WeaponTipPlugin.log";

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        spdlog::info("WeaponTipPlugin logging initialized.");
    }

    // Converte rotação do node em vetor forward (eixo Y = forward)
    static NiPoint3 GetForwardVector(const NiAVObject* node) {
        if (!node) return { 0.0f, 0.0f, 1.0f };

        auto& mat = node->world.rotate;
        return { mat.entry[0][1], mat.entry[1][1], mat.entry[2][1] };
    }

    // Posição da ponta da arma (usando SOMENTE node "WEAPON")
    static NiPoint3 GetWeaponTipPos(Actor* actor) {
        if (!actor) return { 0.0f, 0.0f, 0.0f };

        NiAVObject* root3D = actor->Get3D(false);
        if (!root3D) return { 0.0f, 0.0f, 0.0f };

        // Sempre procura pelo node "WEAPON"
        BSFixedString nodeName("WEAPON");
        NiAVObject* weaponNode = root3D->GetObjectByName(nodeName);
        if (!weaponNode) return { 0.0f, 0.0f, 0.0f };

        // Pega a API do Precision
        auto precisionRaw = PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4);
        auto precision = reinterpret_cast<PRECISION_API::IVPrecision4*>(precisionRaw);
        if (!precision) return { 0.0f, 0.0f, 0.0f };

        RE::ActorHandle actorHandle = actor->GetHandle();

        // Corrige caso o node seja um clone
        NiAVObject* original = precision->GetOriginalFromClone(actorHandle, weaponNode);
        if (original) {
            weaponNode = original;
        }

        // Pega o comprimento da cápsula de ataque
        PRECISION_API::RequestedAttackCollisionType reqType = PRECISION_API::RequestedAttackCollisionType::RightWeapon;

        float length = precision->GetAttackCollisionCapsuleLength(actorHandle, reqType);

        length *= 0.8;

        NiPoint3 origin = weaponNode->world.translate;
        NiPoint3 dir = GetForwardVector(weaponNode);

        // Normaliza
        float mag = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (mag > 0.00001f) {
            dir.x /= mag;
            dir.y /= mag;
            dir.z /= mag;
        } else {
            dir = { 0.0f, 1.0f, 0.0f };
        }

        NiPoint3 tip = origin + (dir * length);
        return tip;
    }

    // Posição da ponta do shield (usando node "SHIELD")
    static NiPoint3 GetShieldTipPos(Actor* actor) {
        if (!actor) return { 0.0f, 0.0f, 0.0f };

        NiAVObject* root3D = actor->Get3D(false);
        if (!root3D) return { 0.0f, 0.0f, 0.0f };

        BSFixedString nodeName("SHIELD");
        NiAVObject* shieldNode = root3D->GetObjectByName(nodeName);
        if (!shieldNode) return { 0.0f, 0.0f, 0.0f };

        auto precisionRaw = PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4);
        auto precision = reinterpret_cast<PRECISION_API::IVPrecision4*>(precisionRaw);
        if (!precision) return { 0.0f, 0.0f, 0.0f };

        RE::ActorHandle actorHandle = actor->GetHandle();

        // Corrige clone → original
        NiAVObject* original = precision->GetOriginalFromClone(actorHandle, shieldNode);
        if (original) {
            shieldNode = original;
        }

        // Para escudos, vamos assumir sempre LeftWeapon (mão esquerda)
        float length = precision->GetAttackCollisionCapsuleLength(
            actorHandle,
            PRECISION_API::RequestedAttackCollisionType::LeftWeapon
        );

        length *= 0.8;

        NiPoint3 origin = shieldNode->world.translate;
        NiPoint3 dir = GetForwardVector(shieldNode);

        // Normaliza
        float mag = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (mag > 0.00001f) {
            dir.x /= mag;
            dir.y /= mag;
            dir.z /= mag;
        } else {
            dir = { 0.0f, 1.0f, 0.0f };
        }

        NiPoint3 tip = origin + (dir * length);
        return tip;
    }

    static NiPoint3 GetWeaponTipPosOpposite(Actor* actor) {
        if (!actor) return { 0.0f, 0.0f, 0.0f };

        NiAVObject* root3D = actor->Get3D(false);
        if (!root3D) return { 0.0f, 0.0f, 0.0f };

        BSFixedString nodeName("WEAPON");
        NiAVObject* weaponNode = root3D->GetObjectByName(nodeName);
        if (!weaponNode) return { 0.0f, 0.0f, 0.0f };

        auto precisionRaw = PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4);
        auto precision = reinterpret_cast<PRECISION_API::IVPrecision4*>(precisionRaw);
        if (!precision) return { 0.0f, 0.0f, 0.0f };

        RE::ActorHandle actorHandle = actor->GetHandle();

        NiAVObject* original = precision->GetOriginalFromClone(actorHandle, weaponNode);
        if (original) {
            weaponNode = original;
        }

        PRECISION_API::RequestedAttackCollisionType reqType = PRECISION_API::RequestedAttackCollisionType::RightWeapon;

        float length = precision->GetAttackCollisionCapsuleLength(actorHandle, reqType);
        length *= 0.8;

        NiPoint3 origin = weaponNode->world.translate;
        NiPoint3 dir = GetForwardVector(weaponNode);

        // Normalize
        float mag = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (mag > 0.00001f) {
            dir.x /= mag;
            dir.y /= mag;
            dir.z /= mag;
        } else {
            dir = { 0.0f, 1.0f, 0.0f };
        }

        // OPPOSITE DIRECTION: subtract instead of add
        NiPoint3 tip = origin - (dir * length);
        return tip;
    }

    // Helper: cast spell from marker to marker, actor is blame source
    static void DoSpellCastAtPos(Actor* caster, SpellItem* spell, TESBoundObject* markerBase, const NiPoint3& pos, bool snapToGround)
    {
        if (!caster || !spell || !markerBase) return;

        // Place the marker
        NiPointer<TESObjectREFR> marker = caster->PlaceObjectAtMe(markerBase, false);
        if (!marker) return;

        if (snapToGround) {
            // Use weapon tip's XY but actor's Z (ground level)
            NiPoint3 groundPos = pos;
            groundPos.z = caster->GetPositionZ();
            marker->SetPosition(groundPos);
        }
        else
        // Move it to the tip position
        marker->SetPosition(pos);

        // Cast spell from marker to marker, blaming actor
        if (auto magicCaster = marker->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            magicCaster->CastSpellImmediate(
                spell,
                false,      // noHitEffectArt
                marker.get(),     // target
                1.0f,       // effectiveness
                false,      // hostileEffectivenessOnly
                0.0f,       // magnitudeOverride
                caster      // blame actor
            );
        }

        // After the spell cast, mark for deletion
        marker->SetDelete(true);
    }


    // Papyrus: right/left hand weapon
    void CastSpellAtWeaponTip(StaticFunctionTag*, Actor* akActor, SpellItem* akSpell, TESBoundObject* akMarkerBase, bool snapToGround)
    {
        if (!akActor || !akSpell || !akMarkerBase) return;
        NiPoint3 pos = GetWeaponTipPos(akActor);
        DoSpellCastAtPos(akActor, akSpell, akMarkerBase, pos, snapToGround);
    }

    // Papyrus: shield
    void CastSpellAtShieldTip(StaticFunctionTag*, Actor* akActor, SpellItem* akSpell, TESBoundObject* akMarkerBase, bool snapToGround)
    {
        if (!akActor || !akSpell || !akMarkerBase) return;
        NiPoint3 pos = GetShieldTipPos(akActor);
        DoSpellCastAtPos(akActor, akSpell, akMarkerBase, pos, snapToGround);
    }

    void CastSpellAtWeaponTipOpposite(StaticFunctionTag*, Actor* akActor, SpellItem* akSpell, TESBoundObject* akMarkerBase, bool snapToGround)
    {
        if (!akActor || !akSpell || !akMarkerBase) return;
        NiPoint3 pos = GetWeaponTipPosOpposite(akActor);
        DoSpellCastAtPos(akActor, akSpell, akMarkerBase, pos, snapToGround);
    }


    bool RegisterFuncs(BSScript::IVirtualMachine* vm) {

        // New native exposed functions
        vm->RegisterFunction("CastSpellAtWeaponTip", "_WeaponTipBridge", CastSpellAtWeaponTip);
        vm->RegisterFunction("CastSpellAtShieldTip", "_WeaponTipBridge", CastSpellAtShieldTip);
        vm->RegisterFunction("CastSpellAtWeaponTipOpposite", "_WeaponTipBridge", CastSpellAtWeaponTipOpposite);

        return true;
    }
}

SKSEPluginLoad(const LoadInterface* skse) {
    Init(skse);

    WeaponTip::InitLogging();

    auto papyrus = GetPapyrusInterface();
    if (papyrus) {
        papyrus->Register(WeaponTip::RegisterFuncs);
    }

    spdlog::info("WeaponTipPlugin loaded successfully.");
    return true;
}
