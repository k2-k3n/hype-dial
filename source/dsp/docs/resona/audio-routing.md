# Routing device audio

Box App platform API4 appends `audio_routing_set/get`. The public declarations
and bounded graph helpers are in `sdk/include/resona/routing.h`. Create/zero a
`resona_audio_routing` with `resona_audio_routing_init`, add connections with
`resona_audio_route_add`, then call `P->audio_routing_set(&graph)`. Check the return
code. Passing NULL restores default routing; `audio_routing_get` copies the
current graph/meters into your own memory.

Sources: GUITAR(0, mono), USB(1, stereo), BLUETOOTH(2, stereo), SLOT1…5(3…7,
mono), MAIN(8, stereo, capture-only). Destinations: SLOT1…5(0…4, mono), OUTPUT(5,
stereo), USB_CAPTURE(6, stereo). Left/mono is channel0, right channel1. Connections
to one input/channel sum with finite linear gain−32767…32767. Use0.5 per channel
to downmix stereo explicitly. An empty or disabled slot passes input. Preserve
headroom when summing; output limiting does not prevent internal over-range.

There are at most64 connections and five slots. Refer only to configured slots.
No duplicate endpoint/channel pairs, non-finite gains, or feedback cycles. Slots
remain mono; this API does not add stereo module instances. The platform derives
processing order, keeps private buffers and invokes each isolated slot once.

Routing is live state: boot/reset and chain mode/count changes restore the normal
guitar chain plus post-chain USB/Bluetooth playback. It is not stored in presets.
Configure slots/load a preset first, then apply the graph. A Box App can set its
graph in `init()` to restore it after boot. Setting/getting graphs is a control
operation; never call it from an algorithm's audio callback.

Creator Lab offers **Device → Routing** with templates, connections, gain, meters,
Apply, Reset and JSON Save/Load. CLI: `resona-lab routing status|check|apply|save|reset`.
Hardware requires matching API4 firmware; host App preview only models graph
configuration/UI. Existing module ABI1 and RBX1 package target are unchanged.

The SDK's `apps/audio_routing` is a complete example. Load an algorithm into its
Slot1 using Lab to hear guitar, USB or Bluetooth processed through it. The source
uses public APIs and does not save presets or overwrite user storage.
