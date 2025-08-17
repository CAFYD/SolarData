# Solar Data Banner Utility

A lightweight, standalone Windows utility that displays real-time solar and geophysical data images from [hamqsl.com](https://www.hamqsl.com/solar.html) directly on your desktop. The application is controlled via a system tray icon and features multiple, independent, movable banner windows.

![Screenshot of SolarData in action]([https://i.imgur.com/4qG4hJz.png](https://imgur.com/a/LqgipAi))

## Features

*   **Real-time Data:** Fetches and displays multiple, up-to-date solar data images.
*   **Automatic Refresh:** Each banner automatically refreshes with the latest data every 10 minutes.
*   **System Tray Control:** The entire application is managed from a single icon in the system tray (notification area), keeping your taskbar clean.
*   **On-Demand Windows:** Show or hide specific data banners through a simple right-click menu on the tray icon.
*   **Auto-Start:** Key banners are displayed automatically when the application starts.
*   **Movable Windows:** Click and drag any part of a banner image to move its window to your preferred location on the screen.
*   **Borderless Design:** By default, the banner windows are borderless for a clean look.
*   **Interactive Frame:** The standard window frame (title bar and borders) appears automatically when you hover the mouse over a banner, allowing you to resize or close it with the 'X' button.
*   **Persistent Positioning:** Once moved, a banner window will remember its position.
*   **Flicker-Free & Efficient:** Built with the native Win32 API and GDI+ for minimal resource usage and smooth, flicker-free rendering.

## Requirements

*   Windows XP or later.
*   An active internet connection to download the images.

## How to Compile

To compile this project from source, you will need:

*   **Microsoft Visual Studio** (any recent version, e.g., 2019 or 2022).
*   The **"Desktop development with C++"** workload installed via the Visual Studio Installer.

Follow these steps:
1.  Create a new, empty **"Windows Desktop Application"** project in Visual Studio.
2.  Replace the content of the main `.cpp` file with the code from `SolarData.cpp`.
3.  **Add an Icon:**
    *   In the **Solution Explorer**, right-click on **Resource Files -> Add -> Resource...**.
    *   Select **Icon** and click **Import...**.
    *   Choose a `.ico` file. Visual Studio will create a `resource.h` file and assign an ID to your icon (e.g., `IDI_SOLARDATA` or `IDI_ICON1`).
    *   Ensure the ID in the code matches the ID in `resource.h`.
4.  **Build the solution** (F7 or `Build -> Build Solution`). The `SolarData.exe` file will be created in your project's `Debug` or `Release` folder.
5.  **Customize (Optional):** You can easily change the banner list, menu titles, and the `refreshMinutes` constant by editing the global variables at the top of the `SolarData.cpp` file before compiling.

## How to Use

1.  Run `SolarData.exe`.
2.  The application icon will appear in your system tray.
3.  The pre-selected banner windows will appear stacked in the bottom-right corner of your screen.
4.  **Drag** any banner to move it.
5.  **Hover** over a banner to show its frame and title bar. Use the 'X' to hide it.
6.  **Right-click** the system tray icon to open the menu.
    *   A checkmark indicates a visible banner.
    *   Click a menu item to toggle a banner's visibility.
    *   Click **Exit** to close the application completely.
