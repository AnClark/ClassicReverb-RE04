#include "UI.h"

// ── _handleFileBrowserIdle ────────────────────────────────────────────────────
// Must be called every frame from onImGuiDisplay().
// Polls the native file dialog; when the user confirms a selection, performs
// the corresponding Import or Export operation and cleans up the handle.
void ClassicReverbUI::_handleFileBrowserIdle()
{
    if (fFileBrowserHandle == nullptr)
        return;

    // fileBrowserIdle() returns true once the dialog has been closed
    // (either by the user confirming a path, or by cancelling).
    if (!DGL_NAMESPACE::fileBrowserIdle(fFileBrowserHandle))
        return;

    // Retrieve the chosen path (nullptr when the user cancelled).
    const char* selectedPath = DGL_NAMESPACE::fileBrowserGetPath(fFileBrowserHandle);

    if (selectedPath != nullptr && selectedPath[0] != '\0')
    {
        switch (fFileBrowserAction)
        {
            case FileBrowserAction::Import:
                if (fPresetManager->importFromFile(selectedPath)) {
                    const Preset* imported = fPresetManager->currentPreset();
                    const auto message = std::string("Imported preset: ")
                                     + (imported ? imported->name : "")
                                     + "\nDon't forget to click 'Save As' to keep it!";
                    _showMessageBox(message);
                } else {
                    _showMessageBox("Import failed: cannot read file.");
                }
                break;

            case FileBrowserAction::Export:
                if (fPresetManager->exportCurrentToFile(selectedPath))
                    _showMessageBox("Preset exported.");
                else
                    _showMessageBox("Export failed: cannot write file.");
                break;

            default:
                break;
        }
    }

    // Always clean up the dialog handle after it has been closed.
    DGL_NAMESPACE::fileBrowserClose(fFileBrowserHandle);
    fFileBrowserHandle = nullptr;
    fFileBrowserAction = FileBrowserAction::None;
}
