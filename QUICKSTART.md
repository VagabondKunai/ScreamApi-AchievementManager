# QUICK START GUIDE - ScreamAPI v1.13 Fixed

## 🚀 What's Fixed

Your ScreamAPI now works with EOS SDK v1.16.3 games (like Beholder).

**Key fixes:**
1. ✅ Missing struct fields added (LocalUserId parameters)
2. ✅ Platform polling instead of failed manual creation
3. ✅ Interface caching removed (was keeping NULL values)
4. ✅ Achievement manager initialization triggered when platform ready

---

## 📦 Build & Install

### Build (Windows + Visual Studio):
```batch
1. Open ScreamAPI.sln
2. Select: Release | Win32 (for 32-bit games)
3. Build Solution (Ctrl+Shift+B)
4. Output: .output\Win32\Release\EOSSDK-Win32-Shipping.dll
```

### Install to Game:
```batch
1. Find game's EOSSDK-Win32-Shipping.dll
2. Rename it to: EOSSDK-Win32-Shipping_o.dll
3. Copy ScreamAPI's DLL as: EOSSDK-Win32-Shipping.dll
4. Edit ScreamAPI.ini:
   [Config]
   EnableOverlay=true
```

---

## 🎮 Usage

1. Launch game
2. Wait 3-10 seconds for platform detection
3. Press **INSERT** or **HOME** key to open overlay
4. View & unlock achievements!

---

## 📊 Expected Log Output (Success)

```
[INFO]  Successfully loaded original EOS SDK
[INFO]  Waiting for game to create EOS Platform via EOS_Platform_Create hook
[INFO]  EOS_Platform_Create called - setting hPlatform
[INFO]  EOS_Platform_Create result: 0x12AB34CD  ← Platform created!
[INFO]  EOS Platform detected as ready after 3 seconds
[UTIL]  Achievements Int: 0xABCDEF00 OK  ← Interface working!
[INFO]  [ACH] Platform is ready - proceeding with initialization
[DEBUG] [ACH] Calling EOS_Achievements_QueryDefinitions
[INFO]  Found 25 achievement definitions  ← SUCCESS!
```

---

## ❌ Troubleshooting

### Platform stays NULL after 60 seconds?
- Game may not use EOS Achievements
- Check: Does game actually have achievements?

### No achievements found (0 definitions)?
- Game has no achievements configured
- This is normal for some games

### Overlay doesn't appear?
- Check `EnableOverlay=true` in ScreamAPI.ini
- Try different hotkeys: INSERT, HOME, F12
- Verify game uses DirectX 11

---

## 📁 Files Modified

Critical changes to these files:
- `ScreamAPI/src/util.cpp` - Removed static caching
- `ScreamAPI/src/util.h` - Added helper functions
- `ScreamAPI/src/achievement_manager.cpp` - Fixed struct fields
- `ScreamAPI/src/eos-impl/eos_init.cpp` - Added init trigger
- `ScreamAPI/src/ScreamAPI.cpp` - Platform polling

See **CHANGES.md** for detailed technical documentation.

---

## 🎯 Tested With

- ✅ Beholder (32-bit) - EOS SDK v1.16.3
- ✅ Works with v1.13-v1.16 SDK versions

---

## 🔍 Debugging

Check **ScreamAPI.log** for diagnostics:

**Good signs:**
- "EOS_Platform_Create result: 0x<ADDRESS>" (not NULL)
- "Achievements Int: 0x<ADDRESS> OK"
- "Found N achievement definitions"

**Bad signs:**
- "Platform still NULL after"
- "HAchievements is NULL"
- "Cannot query achievements"

If you see bad signs:
1. Game may not have achievements
2. Game may use newer SDK version (>v1.16.3)
3. Check if game even uses EOS

---

Need help? Check CHANGES.md for full technical details.
