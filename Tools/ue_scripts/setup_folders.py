"""Creates the Saltbound content folder layout (idempotent). See Docs/Conventions.md."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

FOLDERS = [
    "/Game/Saltbound/Core",
    "/Game/Saltbound/Input",
    "/Game/Saltbound/Player",
    "/Game/Saltbound/Boat",
    "/Game/Saltbound/Water",
    "/Game/Saltbound/Environment/Harbor",
    "/Game/Saltbound/Environment/Rocks",
    "/Game/Saltbound/Environment/Sky",
    "/Game/Saltbound/Props",
    "/Game/Saltbound/Materials/Master",
    "/Game/Saltbound/Materials/Functions",
    "/Game/Saltbound/Materials/Instances",
    "/Game/Saltbound/Materials/Textures",
    "/Game/Saltbound/Audio",
    "/Game/Saltbound/UI",
    "/Game/Saltbound/Maps",
    "/Game/Saltbound/Dev",
    "/Game/ThirdParty",
]
for f in FOLDERS:
    sb_ue.ensure_folder(f)
print("SB_FOLDERS_OK", len(FOLDERS))
