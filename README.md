## Requirements

- Skyrim Special Edition (SE/AE/VR)
- SKSE
- XMake 3.0.1+
- C++23 Compiler (MSVC or Clang-CL)

## Building

### Clone the Repository
```batch
git clone --recurse-submodules <repository-url>
cd DisabledReferenceIntegrityFix
```

### Build with XMake
```batch
xmake build
```

The compiled DLL will be generated in `build/windows/release/`.

### Generate Visual Studio Project (Optional)
```batch
xmake project -k vsxmake
```

### Upgrade Dependencies (Optional)
```batch
xmake repo --update
xmake require --upgrade
```

## Building Output

Set environment variables to redirect build output:

- `XSE_TES5_GAME_PATH`: Path to Skyrim SE installation
- `XSE_TES5_MODS_PATH`: Path to Mod Manager mods folder

## Technical Details

- **Framework**: CommonLibSSE-NG
- **Language**: C++23
- **API**: SKSE64 Plugin Interface
- **Build System**: XMake
