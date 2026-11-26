# UI Login Redesign — Phase 3B implementation

This document describes code that is now in `RANMobile/UI/Login/LoginScreen.cpp/.h` and `RANMobile/UI/Foundation/MobileUIKit.cpp/.h`. It is not a proposal or a screenshot mockup.

## Implemented layout

`LoginScreen::ComputeLayout()` computes one `LoginLayout` used by both hit testing and drawing, so visuals and touch targets cannot drift apart. Coordinates are virtual design units produced by `RenderLayout`:

- **Landscape (1280×720 virtual canvas)**: preserves the original EP7 control anchors from the pre-Phase-3B screen, so existing tests and muscle memory remain valid. Login/register/settings/exit rows use a minimum ~48-unit height.
- **Portrait (720×1280 virtual canvas)**: full-width stacked form. Fields are 56 units, primary actions 64 units, secondary actions 56 units. The panel is centered inside the design canvas and respects safe-area insets.
- **Safe area**: `SetSafeAreaInsets(left, top, right, bottom)` in virtual pixels; default zero because no Java DisplayCutout bridge exists yet.

## Implemented visual system (shared `MobileUIKit`)

- `DrawPanel`, `DrawButton`, `DrawStatusPill`, `HitTest`, `CenteredRect`, `InsetRect`.
- `MobileUITheme` defines accent, panel, text colors, padding and a 48-unit minimum target.
- `ButtonVisualState` covers `Normal`, `Focused`, `Pressed`, `Disabled`, `Loading`; pressed/focused states add an accent rail and label color change.
- Login screen adds layered background bands, a title/accent rule, a server label, focus glow on active fields, a show/hide control, checkbox rendering, status pill and dialogs. All are drawn with the existing `MobileRenderer` quad/text primitives — no substitute game art was generated.

## Implemented behaviour

| Control | Behaviour |
|---|---|
| Username / password fields | Tap to focus; hardware key events append UTF-8 through `RANMobileEngine::OnTextInput`; 20-character cap retained from original field lengths. |
| Show / hide password | Dedicated `SHOW`/`HIDE` button; `IsPasswordVisible()` and `GetPasswordToggleRect()` expose state for tests. |
| Remember account | Checkbox toggles presentation-only `m_rememberAccount`; no persistence or server change. |
| Login | Validates empty input, guards repeated taps while a request is pending, calls the unchanged `AuthenticationManager::Login`. |
| Register | Opens an informational panel telling the user to register on the FAM HAWAII website; no fake account creation. |
| Settings | Sound/music/graphics toggles are local presentation state; graphics level does not yet call `ApplySettings` (documented limitation). |
| Exit / back | Opens a confirmation dialog; YES sets `WantsExit()` for the native activity, NO cancels. `HandleTouchCancel()` clears pressed state. |
| Status | Colored pill reflects `Disconnected`, `LoginRejected`, `ConnectionError`, `Authenticating` and `AwaitingServerResponse`. |

## Protocol safety

After a successful socket write the state is `AwaitingServerResponse` with the text `REQUEST SENT - SERVER RESPONSE NOT IMPLEMENTED`. It is explicitly **not** `LoginAccepted`. When the client is not connected, the status is `SERVER NOT CONNECTED` and no login success is asserted. Phase 3A's `LOGIN_2` uncertainty is unchanged; no protocol constant or payload was touched.

## Tests

`RANMobile/Tests/test_main.cpp` sections 15 and 17 retain the original landscape hit-test expectations; section 24 verifies password toggle, validation, accurate connection status, no false `LoginAccepted`, portrait reflow and return to landscape.
