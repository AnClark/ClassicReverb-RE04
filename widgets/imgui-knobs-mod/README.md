# ImGui Knobs Mod

This is a modded version of [imgui-knobs](https://github.com/altschuler/imgui-knobs), tailored for Classic Series RE Plugins.

## Changes

- Enhanced visual design to match the aesthetic of Kjaerhus Classic Series Plugins.
  - Shadows and highlights added for a more three-dimensional look.
- Implemented tagged labels (like `Knob##UniqueID`) to ensure unique identifiers for each knob, preventing conflicts in ImGui.
- Added support for showing labels below the knobs for better readability.
- Supported custom scale marks on the knobs, allowing for more precise control and visual feedback.

Modifications were made by AnClark, with the help of Claude Sonnet 4.6. See subdirectory `copilot-prompts` for more details.

## Usage

Usage is the same as the original `imgui-knobs` library, with the added features mentioned above. You can use the `ImGuiKnobs_Mod::Knob` function to create knobs in your ImGui interface, and customize their appearance and behavior using the new options.

## License

This mod is licensed under the MIT License, which is the same as the original `imgui-knobs` library.
