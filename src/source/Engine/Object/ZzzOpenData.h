#pragma once

void DeleteNpcs();
void OpenNpc(int Type);
void DeleteMonsters();
void OpenMonsterModel(EMonsterModelType Type);

void OpenModel(int Type, wchar_t* Dir, wchar_t* ModelFileName, ...);
void OpenBasicData(HDC hDC);
void ReleaseMainData();
bool OpenFont();
void OpenLogoSceneData();
void ReleaseLogoSceneData();
void OpenCharacterSceneData();
void ReleaseCharacterSceneData();

void OpenPlayerTextures();
void OpenItemTextures();

#include <cstdint>

// The skill/item hotkey layout and a few options, kept by the server per character (OpenMU KeyConfiguration).
void SaveOptions();
// Sends the layout only when it differs from what the server holds; false when nothing was sent.
bool SaveOptionsIfChanged();
// SaveOptionsIfChanged() at most once per interval: called every frame in the world, so a change is saved
// right away and closing the window, the tab or a crash loses nothing.
void SaveOptionsIfChangedEvery(std::uint32_t intervalMs);
// The layout now shown is the one the server holds (it has just sent it).
void MarkOptionsSaved();
// No layout from the server yet (entering the world): send nothing until it arrives.
void ForgetSavedOptions();
void SaveMacro(const wchar_t* FileName);
void OpenMacro(const wchar_t* FileName);

extern wchar_t AbuseFilter[][20];
extern wchar_t AbuseNameFilter[][20];
extern int AbuseFilterNumber;
extern int AbuseNameFilterNumber;