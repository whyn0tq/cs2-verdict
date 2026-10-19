#pragma once

#include <string>

void PlaySoundAt(int slot, const std::string& sound);
void StartFireEffect(int slot);
void StopFireEffect(int slot);
void ShowExplosion(int slot);
void ClearAllEffects();
