# ImGui Extensions for Classic Series RE

This directory contains a collection of custom widgets and utilities for enhancing user interfaces of Classic Series RE. These components are designed to provide additional functionality and visual styles beyond the standard ImGui library.

## Components

### 1. AddTextScaled

- **Header File**: `AddTextScaled.hpp`
- **Description**: Provides a utility to render scaled text in ImGui. It supports independent scaling along the X and Y axes, ensuring text fits within specific layouts or designs.
- **Usage**: Use `ImGuiExt::AddTextScaled` to draw text with custom scaling factors.

### 2. CenteredSeparatorText

- **Header File**: `CenteredSeparatorText.hpp`
- **Description**: A variant of ImGui's `SeparatorText` that centers the label horizontally within the widget area. It also allows adjustable widget width. This is mainly used for section headers of Classic Series RE.
- **Usage**: Use `ImGuiExt::CenteredSeparatorText` to create visually distinct section headers.

### 3. GradientButton

- **Header File**: `GradientButton.hpp`
- **Description**: Implements a button with a vertical gradient background. The gradient colors, hover effects, and active states are customizable. Currently not used in Classic Series RE.
- **Usage**: Use `ImGuiExt::GradientButton` for visually appealing buttons with gradient effects.

### 4. HardwareButton

- **Header File**: `HardwareButton.hpp`
- **Description**: A realistic 3D push-button widget designed for audio plugin UIs. It features a soft drop shadow, gradient rim, and mechanical depression effects. This is mainly used for interactive controls in Classic Series RE.
- **Usage**: Use `ImGuiExt::HardwareButton` for interactive buttons with a hardware-like appearance.

## License

Code in this subdirectory is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
