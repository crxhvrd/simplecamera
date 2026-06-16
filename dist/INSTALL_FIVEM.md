# Running SimpleCamera under FiveM

Use the **`SimpleCamera_FiveM.asi`** build (from `bin\Release_FiveM\`), **not** the
regular `SimpleCamera.asi`. The FiveM build has the raw game-memory code compiled
out and imports only the ScriptHookV functions FiveM's compatibility layer
actually provides — the plain build imports `getScriptHandleBaseAddress`, which
FiveM does not export, so FiveM refuses to load it ("Couldn't load ...").

## Why Windows blocks it ("we can't confirm who published")

The `.asi` is a DLL. When the FiveM subprocess loads it, Windows checks the
publisher. An unsigned (or untrusted-publisher) DLL gets blocked by **Smart App
Control / SmartScreen / Defender**, and FiveM then reports *"Couldn't load
SimpleCamera_FiveM.asi"*. There are two independent gates:

### 1. Smart App Control (SAC) — must be OFF
SAC only trusts Microsoft-reputed / specially-signed code; it ignores
self-signed certificates entirely. If SAC is on, you must turn it off:

> Windows Security → **App & browser control** → **Smart App Control** → **Off**

**Reboot afterwards.** The setting does not fully take effect until you restart —
this is why "turning it off" can appear to do nothing at first.

(SAC is a one-way switch: re-enabling it requires resetting Windows. Any unsigned
game mod is incompatible with SAC, so it has to be off to mod at all.)

### 2. Publisher trust — install the signing certificate
The `.asi` is Authenticode-signed with a self-signed certificate. Tell Windows to
trust that publisher by installing **`SimpleCamera_CodeSign.cer`** on the PC that
runs FiveM:

1. Double-click `SimpleCamera_CodeSign.cer`.
2. **Install Certificate…** → Store location: **Local Machine** → Next (accept UAC).
3. **Place all certificates in the following store** → **Browse** →
   **Trusted Root Certification Authorities** → OK → Next → Finish → **Yes** to the
   security warning.
4. Repeat steps 1–3, this time choosing the **Trusted Publishers** store.

After both gates are cleared, copy `SimpleCamera_FiveM.asi` into FiveM's
`plugins\` folder and launch.

## Re-signing after a rebuild
Run `sign.ps1` from the project root after building. It reuses the same
certificate (so the already-installed `.cer` stays valid) and re-signs the
binaries. If you ever build on a *different* machine, `sign.ps1` will generate a
new certificate — re-export and re-install the new `.cer`.

## Known limitation under FiveM
FiveM's ScriptHookV shim stubs out `worldGetAllVehicles/Peds/Objects` (they
return nothing), so features that enumerate world entities — follow/aim targeting
and "stream around camera" LOD loading — may be inert under FiveM. The core
sequence/keyframe camera, rendering, and native-based wheel spin are unaffected.
