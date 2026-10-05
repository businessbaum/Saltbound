"""The sound groups and the volume mix (D66): the settings' Audio page sets their volumes.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/setup_audio.py
(or headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file>)

  SC_Master       the root class; the master volume itself is the audio device's primary volume
  SC_Music        music (on land, at sea, in the menus)
  SC_SeaAndWind   waves, the wind in the rigging, water along the hull
  SC_Effects      everything else (ropes, footsteps, the satchel, the windlass, the bell); also the default class
                  (DefaultEngine.ini [/Script/Engine.AudioSettings] DefaultSoundClassName), so a sound without one is an effect
  SM_Volumes      the mix the game pushes once and overrides the three groups' volumes in
                  (USaltGameUserSettings::ApplyExtras)

New sounds: set their Sound Class to SC_Music or SC_SeaAndWind where they belong; effects need nothing.
Existing assets are kept and only their links are put right, so the script can run again.
"""
import unreal

FOLDER = "/Game/Saltbound/Audio"
GROUPS = ["SC_Music", "SC_SeaAndWind", "SC_Effects"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def get_or_create(name, cls, factory):
    path = f"{FOLDER}/{name}"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, FOLDER, cls, factory)


def main():
    master = get_or_create("SC_Master", unreal.SoundClass, unreal.SoundClassFactory())
    groups = [get_or_create(n, unreal.SoundClass, unreal.SoundClassFactory()) for n in GROUPS]
    # The children list is editable; the class's edit hook sets the parent of the child just added, so add them one at a time.
    master.set_editor_property("child_classes", [])
    for i in range(len(groups)):
        master.set_editor_property("child_classes", groups[: i + 1])
    mix = get_or_create("SM_Volumes", unreal.SoundMix, unreal.SoundMixFactory())
    for asset in [master, *groups, mix]:
        eal.save_loaded_asset(asset, only_if_is_dirty=False)
    for g in groups:
        parent = g.get_editor_property("parent_class")
        print("SB_AUDIO_CLASS", g.get_name(), "parent", parent.get_name() if parent else None)
    print("SB_AUDIO_DONE", master.get_path_name(), mix.get_path_name())


main()
