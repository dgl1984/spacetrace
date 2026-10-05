# SpaceTrace universal-access UI contract

This contract applies to the SpaceTrace 1.0 editor and future UI work.

## One parameter, one user-facing control

A user-adjustable parameter has exactly one user-facing editor control. SpaceTrace must not create a visual control and a separate accessibility-only control for the same parameter, nor overlap controls that edit the same value.

The same control must serve mouse, keyboard, screen-reader, exact-entry, and host-automation workflows wherever the format and host permit.

## Visualisations are output-only

A visualisation may reflect one or more parameter values but is not another way to edit them.

The spatial display is therefore:

- non-focusable;
- excluded from the accessibility hierarchy;
- non-interactive with the mouse;
- not attached to parameters as a control;
- not allowed to write processor or parameter state.

Azimuth, Distance, Elevation, and Stereo Pair Width Offset remain the sole editor controls for those parameters. The spatial display only shows their current values. In Stereo Pair it may split the source marker into two read-only L/R points, but those points never become controls.

## Exact entry is temporary replacement, not duplication

When Enter or F2 opens exact numeric entry, the owning slider is temporarily hidden and a text editor occupies that same control slot. The slider returns when entry ends. They are never simultaneously exposed as two controls for one parameter.

## No explicit text-to-speech layer

SpaceTrace relies on JUCE accessibility and the screen reader/host. It must not speak control changes through OneCore, MSAPI, NVDA-specific speech calls, or another parallel TTS path.

## Decorative and informational content

Purely decorative graphics and labels are excluded from the accessibility hierarchy. Information that is necessary to understand operation must also be available through ordinary accessible component names/descriptions/help text; it must not exist only as painted graphics.

## Normal navigation order

Head Model comes first, followed by Renderer, Input Mode, Tonal Compensation, Load Custom IR, Azimuth, Elevation, Distance, Width Offset, Air Loss, Output, and Bypass. Use normal Tab/Shift+Tab navigation and skip disabled controls. Width Offset is unavailable in Mono Point Source and Papa Pan. Enter/F2 exact entry returns focus to the owning slider when it ends.

## Generic host parameters remain complete

All essential adjustable processing parameters remain available through the host's generic parameter interface. The custom editor is an enhancement, not a requirement for operating the processor.
