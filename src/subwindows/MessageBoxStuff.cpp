#include "UI.h"

// ── _handleMessageBoxIdle ────────────────────────────────────────────────────
// Must be called every frame from onImGuiDisplay().
// Polls the message queue; when a message is present, opens a modal popup to show it.
void ClassicReverbUI::_handleMessageBoxIdle()
{
    if (!fMessageBoxQueue.empty())
    {
        fRequestMessagePopup = true;
    }

    if (fRequestMessagePopup)
    {
        ImGui::OpenPopup("Message");
        fRequestMessagePopup = false;
    }

    // Always center this window when appearing
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Message", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("%s", fMessageBoxQueue.front().c_str());
        ImGui::Dummy(ImVec2(0, 10));

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(60, 0)))
        {
            // Apply a mutex to avoid possible conflict
            std::lock_guard<std::mutex> lock(fMessageQueueMutex);

            fMessageBoxQueue.pop();

            ImGui::CloseCurrentPopup();
            fRequestMessagePopup = false;
        }
        ImGui::SetItemDefaultFocus();

        ImGui::EndPopup();
    }
}
