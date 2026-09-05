# Button sprite preparation

All ordinary buttons use GrayUiRenderer::drawButton, including both UiButton instances and immediate management-panel controls. GrayUiRenderer owns an optional atlas registry with shared texture lifetimes. No artwork is required yet and the existing appearance remains the fallback.

Register ButtonSpriteSkin with GrayUiRenderer::setButtonSkin(id, skin). The six atlas frames are Normal, Hovered, Pressed, Selected, SelectedHovered, Disabled. A missing state uses the normal frame; a missing normal frame uses the whole texture. Disabled fallback receives a dark overlay. Optional sliceBorder enables nine-slice scaling so borders retain their thickness. UI bounds and click handling do not depend on sprite dimensions.

A registered `default` skin covers the whole game. UiButton::setSkinId selects an explicit stable ID; otherwise the button label is its lookup key. Immediate drawButton calls accept an optional skin ID as the last argument. Captions and contextual icons remain separate foreground content for localization and readable dynamic values.

Dedicated roles include time-pause, time-normal, time-double, time-fast, population, choice-card, color-swatch, debug-minimize, and debug-close. Management controls use their action plus -increase or -decrease. Choice-card and color-swatch surfaces and the normal-font debug buttons also use the same registry; their functional content remains intact.

Clear the registry before destroying or replacing its renderer. No sprite assets or new recipes were introduced by this preparation.
