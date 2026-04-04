#pragma once

namespace HotkeyHandler {
    void Start();   // registers hotkeys and starts message loop thread
    void Stop();    // unregisters and stops the thread
}