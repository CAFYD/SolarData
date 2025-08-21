# SolarData Banner Utility

A lightweight, standalone Windows utility that displays real-time solar data banners from [hamqsl.com](https://www.hamqsl.com/) and NOAA directly on your desktop. The application is controlled via a system tray icon, featuring multiple, independent, movable banner windows.

![Screenshot of SolarData in action](https://i.imgur.com/4qG4hJz.png)

## Features

*   **Stable & Reliable:** Displays the 4 key solar data banners that are verified to work 100% of the time.
*   **System Tray Control:** The entire application is managed from a single icon in the system tray (notification area), keeping your taskbar clean.
*   **Smart Auto-Start:** Shows a default set of banners on the first run, arranged neatly in the corner of your screen.
*   **State Persistence:** The application remembers the position and visibility of each banner. The next time you start it, your custom layout is perfectly restored.
*   **Movable Windows:** Click and drag any part of a banner image to move its window. The first time you move a window, the application switches from auto-layout to your custom layout.
*   **Borderless Design:** By default, the banner windows are borderless for a clean look.
*   **Interactive Frame:** The standard window frame (title bar and borders) appears automatically when you hover the mouse over a banner, allowing you to close it with the 'X' button.
*   **Automatic Refresh:** All data is automatically updated every 10 minutes.
*   **Lightweight & Efficient:** Built with the native Win32 API and GDI+ for minimal resource usage. No admin rights required.

## Requirements

*   Windows XP or later.
*   An active internet connection to download the images.

## How to Compile

The source code can be compiled into a standalone executable using Microsoft Visual Studio.

1.  Open the project in Visual Studio (any recent version, e.g., 2019 or 2022).
2.  Ensure you have the **"Desktop development with C++"** workload installed.
3.  Build the solution (`Build -> Build Solution`), preferably in **Release** mode.

## How to Use

1.  Download the **Setup Installer** or the **Portable ZIP** from the [Releases page](https://github.com/CAFYD/SolarData/releases).
2.  Run the installer or extract the zip and run `SolarData.exe`.
3.  The application icon will appear in your system tray, and the banner windows will appear on your screen.
4.  **Right-click** the system tray icon to toggle banners or to click **Exit** to close the application completely.
