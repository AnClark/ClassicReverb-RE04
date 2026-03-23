// PresetManagerUI.cpp
// Implements _drawPresetManager(), _applyRestoredPresetState() 
// all preset-manager UI logic for ClassicReverbUI.
//
// The companion PresetManager class (PresetManager.cpp) handles data and disk I/O
// without any ImGui dependency.

#include "UI.h"
#include "PresetManager.h"

#include "imgui.h"
#include "FileBrowserDialog.hpp"  // DPF cross-platform file browser API

#include "../fonts/IconFontAwesome5.h"    // For icon glyphs like "X" (close), pencil (edit), floppy disk (save), etc.

#include <cstring>
#include <string>

// ── Colour helpers ────────────────────────────────────────────────────────────
static constexpr auto kColDefault = IM_COL32(0x2f, 0x4d, 0x44, 0xff);
static constexpr auto kColDefaultActive = IM_COL32(0x2f + 20, 0x4d + 20, 0x44 + 20, 0xff);
static constexpr auto kColDefaultHover = IM_COL32(0x2f + 40, 0x4d + 40, 0x44 + 40, 0xff);

static constexpr ImVec4 kColSelected      = { 0.26f, 0.59f, 0.98f, 0.55f };
static constexpr ImVec4 kColSelectedHover = { 0.26f, 0.59f, 0.98f, 0.75f };
static constexpr ImVec4 kColImported      = { 0.55f, 0.80f, 0.35f, 0.55f };
static constexpr ImVec4 kColImportedHover = { 0.55f, 0.80f, 0.35f, 0.75f };
static constexpr ImVec4 kColDanger        = { 0.80f, 0.20f, 0.20f, 1.00f };

// ── _applyRestoredPresetState ─────────────────────────────────────────────────
// Called from stateChanged() after buffering all three state keys.
void ClassicReverbUI::_applyRestoredPresetState()
{
    if (fPresetManager)
        fPresetManager->restoreFromState(fRestoredPresetType, fRestoredPresetName, fRestoredModified);
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawPresetManager
// Called at the end of onImGuiDisplay(). Does nothing when the window is closed.
// ─────────────────────────────────────────────────────────────────────────────
void ClassicReverbUI::_drawPresetManager()
{
    if (!fPresetManagerOpened)
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr float kMargin = 8.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + kMargin, viewport->Pos.y + kMargin));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - 2.0f * kMargin,
                                    viewport->Size.y - 2.0f * kMargin));
    ImGui::SetNextWindowBgAlpha(0.97f);

    // Push compact style so the overlay fits in the narrow plugin window.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(4.0f, 2.0f));

    // Enable rounding for UI elements
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

    // Apply our color theme
    ImGui::PushStyleColor(ImGuiCol_Button, kColDefault);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kColDefaultActive);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kColDefaultHover);

    constexpr auto kPmFlags =
        ImGuiWindowFlags_NoTitleBar     |
        ImGuiWindowFlags_NoResize       |
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##PresetManager", nullptr, kPmFlags))
    {
        // Close the overlay when the user clicks outside it.
        // Skip while any popup modal is open (Save As / Rename / Delete / Message Box / …)
        // so that dismissing a modal dialog (e.g. clicking "OK" in the message box)
        // is not mis-interpreted as a "click outside" that would close this window.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows |
                                    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        {
            fPresetManagerOpened = false;
        }

        const float availW = ImGui::GetContentRegionAvail().x;
        const float availH = ImGui::GetContentRegionAvail().y;
        const float gap    = ImGui::GetStyle().ItemSpacing.x;

        // Button height for preset buttons
        constexpr float kBtnH  = 19.0f;
        // Height of the action buttons row at the bottom of the user panel
        constexpr float kActH  = 19.0f;
        const float actionRowH = kActH + ImGui::GetStyle().ItemSpacing.y;

        // ── Column widths ────────────────────────────────────────────────
        const float leftW  = availW * 0.38f;
        const float rightW = availW - leftW - gap;

        // ════════════════════════════════════════════════════════════════════
        // LEFT COLUMN: Factory presets
        // ════════════════════════════════════════════════════════════════════
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 0.50f));
        if (ImGui::BeginChild("##FactoryCol", ImVec2(leftW, availH), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::TextDisabled("FACTORY PRESETS");
            ImGui::Separator();

            // ── Why btnW must be computed INSIDE BeginChild ───────────────
            //
            // Root cause:
            //   When a vertical scrollbar appears, ImGui reserves a strip of
            //   horizontal space for it (≈ ImGui::GetStyle().ScrollbarSize,
            //   typically ~14 px) from within the child window's content area.
            //   If GetContentRegionAvail().x is called BEFORE BeginChild, the
            //   current context is still the parent window and the scrollbar has
            //   not yet been accounted for, so the returned width is too wide.
            //   Buttons sized with that inflated width overflow and get clipped
            //   by the child window's edge or obscured by the scrollbar itself.
            //
            // Fix:
            //   Call GetContentRegionAvail().x AFTER BeginChild succeeds.
            //   At that point ImGui's context has switched to the child window,
            //   and GetContentRegionAvail() returns the net usable width —
            //   already reduced by the scrollbar when one is present.
            //   Buttons computed from this value fill the area exactly whether
            //   or not a scrollbar is showing.
            //
            // General ImGui rule:
            //   Any layout measurement that must reflect a child window's true
            //   inner width (or height) must be taken after BeginChild returns
            //   true, never from the parent context beforehand.
            // ────────────────────────────────────────────────────────────────
            if (ImGui::BeginChild("##FactoryList", ImVec2(0.0f, 0.0f), false))
            {
                // Queried inside the child: scrollbar width (if any) is already
                // subtracted from the value GetContentRegionAvail() returns here.
                const float btnW = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;

                // "Default" is a special entry that resets all params to their
                // compiled-in defaults (index == -1 in the Factory type).
                {
                    const bool sel = (fPresetManager->currentType()  == PresetType::Factory &&
                                      fPresetManager->currentIndex() == -1);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Button,        kColSelected);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kColSelectedHover);
                    }
                    if (ImGui::Button("Default", ImVec2(btnW, kBtnH))) {
                        fPresetManager->loadDefaultPreset();
                        fPresetManagerOpened = false;
                    }
                    if (sel) ImGui::PopStyleColor(2);
                }

                for (int i = 0; i < fPresetManager->factoryPresetCount(); ++i)
                {
                    // "Default" occupies slot 0 on the left; factory presets follow.
                    // Odd-indexed items (1, 3, …) go on the right column.
                    if ((i + 1) % 2 != 0) ImGui::SameLine();

                    const Preset& p   = fPresetManager->factoryPreset(i);
                    const bool    sel = (fPresetManager->currentType()  == PresetType::Factory &&
                                         fPresetManager->currentIndex() == i);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Button,        kColSelected);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kColSelectedHover);
                    }
                    if (ImGui::Button(p.name.c_str(), ImVec2(btnW, kBtnH))) {
                        fPresetManager->selectFactoryPreset(i);
                        fPresetManagerOpened = false;
                    }
                    if (sel) ImGui::PopStyleColor(2);
                }
            }
            ImGui::EndChild(); // FactoryList
        }
        ImGui::EndChild(); // FactoryCol
        ImGui::PopStyleColor(); // ChildBg

        ImGui::SameLine();

        // ════════════════════════════════════════════════════════════════════
        // RIGHT COLUMN: User presets + action buttons
        // ════════════════════════════════════════════════════════════════════
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 0.50f));
        if (ImGui::BeginChild("##UserCol", ImVec2(rightW, availH), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            // Title + close button on the same line.
            // Use FramePadding.y = 0 so the button is exactly text-line height,
            // matching the "FACTORY PRESETS" header row on the left.
            ImGui::TextDisabled("USER PRESETS");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()
                                 + ImGui::GetContentRegionAvail().x - 22.0f);
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 0.0f));
                if (ImGui::Button(ICON_FA_TIMES_CIRCLE "##pmclose", ImVec2(22.0f, 0.0f)))
                    fPresetManagerOpened = false;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Close Preset Manager");
                ImGui::PopStyleVar();
            }
            ImGui::Separator();

            const float innerW = ImGui::GetContentRegionAvail().x;
            const float btnW   = (innerW - gap) * 0.5f;

            // Height available for the preset list (below header, above action row + status)
            const float listH = ImGui::GetContentRegionAvail().y
                              - actionRowH
                              - ImGui::GetStyle().ItemSpacing.y;

            if (ImGui::BeginChild("##UserList", ImVec2(0.0f, listH), false))
            {
                if (fPresetManager->userPresetCount() == 0 &&
                    !fPresetManager->hasImported())
                {
                    ImGui::TextDisabled("  No user presets yet. Use \"Save As\" to create one.");
                }

                for (int i = 0; i < fPresetManager->userPresetCount(); ++i)
                {
                    if (i % 2 != 0) ImGui::SameLine();

                    const Preset& p   = fPresetManager->userPreset(i);
                    const bool    sel = (fPresetManager->currentType()  == PresetType::User &&
                                         fPresetManager->currentIndex() == i);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Button,        kColSelected);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kColSelectedHover);
                    }
                    if (ImGui::Button(p.name.c_str(), ImVec2(btnW, kBtnH))) {
                        fPresetManager->selectUserPreset(i);
                        fPresetManagerOpened = false;
                    }
                    if (sel) ImGui::PopStyleColor(2);
                }

                // Show the imported preset as a normal grid entry (green tint = unsaved).
                // It stays in memory for this instance; use "Save As" to persist it.
                if (fPresetManager->hasImported())
                {
                    const int userCount = fPresetManager->userPresetCount();
                    if (userCount % 2 != 0) ImGui::SameLine();

                    const bool impSel = (fPresetManager->currentType() == PresetType::Imported);
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          impSel ? kColImportedHover : kColImported);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kColImportedHover);
                    const Preset* imp = fPresetManager->importedPreset();
                    if (ImGui::Button(imp->name.c_str(), ImVec2(btnW, kBtnH))) {
                        fPresetManager->selectImportedPreset();
                        fPresetManagerOpened = false;
                    }
                    ImGui::PopStyleColor(2);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Imported — not saved.\nUse \"Save As\" to make it permanent.");
                }
            }
            ImGui::EndChild(); // UserList

            // ── Action buttons (right-aligned) ─────────────────────────────
            constexpr float kWDel    = 46.0f;
            constexpr float kWRen    = 54.0f;
            constexpr float kWUpd    = 50.0f;
            constexpr float kWSaveAs = 56.0f;
            constexpr float kWSep    = 8.0f;
            constexpr float kWImp    = 52.0f;
            constexpr float kWExp    = 52.0f;

            const float totalW = kWDel + gap + kWRen + gap + kWUpd + gap + kWSaveAs
                               + kWSep + kWImp + gap + kWExp;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX()
                                 + ImGui::GetContentRegionAvail().x - totalW);

            const bool isUserPresetActive = (fPresetManager->currentType() == PresetType::User &&
                                        fPresetManager->currentIndex() >= 0);

            // Preset context buttons (only active when a user preset is selected)
            if (!isUserPresetActive) ImGui::BeginDisabled();

            // 1) Delete
            ImGui::PushStyleColor(ImGuiCol_Button, kColDanger);
            if (ImGui::Button("Delete", ImVec2(kWDel, kActH))) {
                fPmDialogMode = PmDialogMode::ConfirmDelete;
                ImGui::OpenPopup("Delete Preset##PM");
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();

            // 2) Rename
            if (ImGui::Button("Rename", ImVec2(kWRen, kActH))) {
                fPmDialogMode = PmDialogMode::Rename;
                const Preset* cur = fPresetManager->currentPreset();
                if (cur) {
                    std::strncpy(fPmNameBuffer, cur->name.c_str(), sizeof(fPmNameBuffer) - 1);
                    fPmNameBuffer[sizeof(fPmNameBuffer) - 1] = '\0';
                }
                ImGui::OpenPopup("Rename Preset##PM");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Rename the current active user preset.");
            ImGui::SameLine();

            // 3) Update (overwrite current preset with current params)
            if (ImGui::Button("Update", ImVec2(kWUpd, kActH))) {
                fPmDialogMode = PmDialogMode::ConfirmUpdate;
                ImGui::OpenPopup("Update Preset##PM");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Overwrite the current user preset with current parameters.");

            if (!isUserPresetActive) ImGui::EndDisabled();
            ImGui::SameLine();

            // 4) Save As
            if (ImGui::Button("Save As", ImVec2(kWSaveAs, kActH))) {
                fPmDialogMode = PmDialogMode::SaveNew;
                const Preset* cur = fPresetManager->currentPreset();
                if (cur) {
                    std::strncpy(fPmNameBuffer, cur->name.c_str(), sizeof(fPmNameBuffer) - 1);
                    fPmNameBuffer[sizeof(fPmNameBuffer) - 1] = '\0';
                } else {
                    std::memset(fPmNameBuffer, 0, sizeof(fPmNameBuffer));
                }
                ImGui::OpenPopup("Save Preset As##PM");
            }

            ImGui::SameLine(0.0f, kWSep);

            // 5) Import
            if (ImGui::Button("Import", ImVec2(kWImp, kActH))) {
                // Open a native OS file-open dialog (non-blocking).
                // The result is processed every frame in _handleFileBrowserIdle().
                if (fFileBrowserHandle == nullptr) {
                    DGL_NAMESPACE::FileBrowserOptions opts;
                    opts.saving = false;
                    opts.title  = "Import Preset";
                    fFileBrowserHandle = DGL_NAMESPACE::fileBrowserCreate(
                        true,
                        getWindow().getNativeWindowHandle(),
                        getScaleFactor(),
                        opts);
                    fFileBrowserAction = FileBrowserAction::Import;
                }
            }
            ImGui::SameLine();

            // 6) Export
            if (!fPresetManager->currentPreset()) ImGui::BeginDisabled();
            if (ImGui::Button("Export", ImVec2(kWExp, kActH))) {
                // Pre-fill the default filename from the current preset name.
                const Preset* cur = fPresetManager->currentPreset();
                std::string defaultName = cur ? cur->name + ".json" : "preset.json";

                // Open a native OS file-save dialog (non-blocking).
                if (fFileBrowserHandle == nullptr) {
                    DGL_NAMESPACE::FileBrowserOptions opts;
                    opts.saving      = true;
                    opts.title       = "Export Preset";
                    opts.defaultName = defaultName.c_str();
                    fFileBrowserHandle = DGL_NAMESPACE::fileBrowserCreate(
                        true,
                        getWindow().getNativeWindowHandle(),
                        getScaleFactor(),
                        opts);
                    fFileBrowserAction = FileBrowserAction::Export;
                }
            }
            if (!fPresetManager->currentPreset()) ImGui::EndDisabled();

            // ════════════════════════════════════════════════════════════
            // Modal popups centred within the overlay window
            // ════════════════════════════════════════════════════════════
            const ImVec2 pmPos  = ImGui::GetWindowPos();
            const ImVec2 pmSize = ImGui::GetWindowSize();
            const ImVec2 pivot  = ImVec2(0.5f, 0.5f);
            const ImVec2 centre = ImVec2(pmPos.x + pmSize.x * 0.5f,
                                          pmPos.y + pmSize.y * 0.5f);

            // ── Save As ──────────────────────────────────────────────────
            ImGui::SetNextWindowPos(centre, ImGuiCond_Always, pivot);
            if (ImGui::BeginPopupModal("Save Preset As##PM", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoMove))
            {
                ImGui::Text("Preset name:");
                ImGui::SetNextItemWidth(260.0f);
                bool doSave = ImGui::InputText("##PMSaveName", fPmNameBuffer,
                                               sizeof(fPmNameBuffer),
                                               ImGuiInputTextFlags_EnterReturnsTrue);
                const bool nameExists = fPresetManager->nameExists(fPmNameBuffer);
                if (nameExists) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.2f, 1.0f));
                    ImGui::Text("A preset named \"%s\" already exists.", fPmNameBuffer);
                    ImGui::PopStyleColor();
                }
                ImGui::Spacing();
                if (doSave || ImGui::Button(nameExists ? "Overwrite##sv" : "Save##sv",
                                            ImVec2(90.0f, 0.0f)))
                {
                    const std::string name(fPmNameBuffer);
                    if (!name.empty()) {
                        if (nameExists) {
                            // Find and overwrite that named preset
                            for (int i = 0; i < fPresetManager->userPresetCount(); ++i) {
                                if (fPresetManager->userPreset(i).name == name) {
                                    fPresetManager->selectUserPreset(i);
                                    fPresetManager->overwriteCurrent();
                                    break;
                                }
                            }
                            _showMessageBox("Preset overwritten: " + name);
                        } else {
                            fPresetManager->saveAsNew(name);
                            _showMessageBox("Saved: " + name);
                        }
                        fPmDialogMode = PmDialogMode::None;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##sv", ImVec2(70.0f, 0.0f))) {
                    fPmDialogMode = PmDialogMode::None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // ── Rename ────────────────────────────────────────────────────
            ImGui::SetNextWindowPos(centre, ImGuiCond_Always, pivot);
            if (ImGui::BeginPopupModal("Rename Preset##PM", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoMove))
            {
                ImGui::Text("New name:");
                ImGui::SetNextItemWidth(260.0f);
                bool doRename = ImGui::InputText("##PMRenameName", fPmNameBuffer,
                                                 sizeof(fPmNameBuffer),
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::Spacing();
                if (doRename || ImGui::Button("Rename##rn", ImVec2(80.0f, 0.0f))) {
                    const std::string name(fPmNameBuffer);
                    if (!name.empty()) {
                        fPresetManager->renameCurrent(name);
                        _showMessageBox("Renamed to: " + name);
                        fPmDialogMode = PmDialogMode::None;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##rn", ImVec2(70.0f, 0.0f))) {
                    fPmDialogMode = PmDialogMode::None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // ── Update confirmation ───────────────────────────────────────
            ImGui::SetNextWindowPos(centre, ImGuiCond_Always, pivot);
            if (ImGui::BeginPopupModal("Update Preset##PM", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoMove))
            {
                const Preset* cur = fPresetManager->currentPreset();
                if (cur)
                    ImGui::Text("Overwrite \"%s\" with current parameters?", cur->name.c_str());
                ImGui::Spacing();
                if (ImGui::Button("Overwrite##upd", ImVec2(80.0f, 0.0f))) {
                    fPresetManager->overwriteCurrent();
                    _showMessageBox("Preset " + cur->name + " updated.");
                    fPmDialogMode = PmDialogMode::None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##upd", ImVec2(70.0f, 0.0f))) {
                    fPmDialogMode = PmDialogMode::None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // ── Delete confirmation ───────────────────────────────────────
            ImGui::SetNextWindowPos(centre, ImGuiCond_Always, pivot);
            if (ImGui::BeginPopupModal("Delete Preset##PM", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoMove))
            {
                const Preset* cur = fPresetManager->currentPreset();
                if (cur)
                    ImGui::Text("Delete \"%s\"?  This cannot be undone.", cur->name.c_str());
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, kColDanger);
                if (ImGui::Button("Delete##dl", ImVec2(70.0f, 0.0f))) {
                    fPresetManager->deleteCurrent();
                    _showMessageBox("Preset deleted.");
                    fPmDialogMode = PmDialogMode::None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("Cancel##dl", ImVec2(70.0f, 0.0f))) {
                    fPmDialogMode = PmDialogMode::None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

        }
        ImGui::EndChild(); // UserCol
        ImGui::PopStyleColor(); // ChildBg
    }
    ImGui::End(); // ##PresetManager

    ImGui::PopStyleColor(3); // Button, ButtonActive, ButtonHovered
    ImGui::PopStyleVar(5); // WindowPadding, ItemSpacing, FramePadding, ChildRounding, FrameRounding
}
