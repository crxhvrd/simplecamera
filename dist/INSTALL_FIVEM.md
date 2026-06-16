# Running SimpleCamera under FiveM

Use the **`SimpleCamera_FiveM.asi`** build (from `bin\Release_FiveM\`), **not** the
regular `SimpleCamera.asi`. The FiveM build has the raw game-memory code compiled
out and imports only the ScriptHookV functions FiveM's compatibility layer
actually provides. The plain build imports `getScriptHandleBaseAddress`, which
FiveM does not export, so FiveM refuses to load it ("Couldn't load ...").

Drop `SimpleCamera_FiveM.asi` into FiveM's `plugins\` folder.

## If Windows blocks it ("we can't confirm who published")

That message is **Smart App Control (SAC)**. SAC blocks unsigned third-party
DLLs that another process (the FiveM subprocess) tries to load. If you hit it:

> Windows Security -> **App & browser control** -> **Smart App Control** -> **Off**

**Reboot afterwards** - the setting does not take effect until you restart, which
is why turning it off can appear to do nothing at first. (SAC is a one-way switch:
re-enabling it requires resetting Windows. Any unsigned game mod is incompatible
with SAC, so it has to be off to mod at all.)

## Known limitation under FiveM

FiveM's ScriptHookV shim stubs out `worldGetAllVehicles/Peds/Objects` (they
return nothing), so features that enumerate world entities - follow/aim targeting
and "stream around camera" LOD loading - may be inert under FiveM. The core
sequence/keyframe camera, rendering, and wheel spin are unaffected.
