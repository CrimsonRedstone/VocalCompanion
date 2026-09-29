#pragma once
#include <functional>
#include "DSP/Common.h"
#include "Chain.h"

inline void showAddModuleMenu (int screenX, int screenY,
                               std::function<void (vc::ModuleType)> onPick)
{
    juce::PopupMenu menu;
    for (int i = 0; i < vc::kPaletteCount; ++i)
    {
        const auto type = vc::kPalette[i];
        juce::PopupMenu::Item it;
        it.itemID = i + 1;
        it.text = vc::typeName (type);
        it.colour = vc::typeColour (type).accent;
        menu.addItem (it);
    }
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenX, screenY, 2, 2 }),
                        [onPick] (int r)
                        {
                            if (r > 0 && onPick)
                                onPick (vc::kPalette[r - 1]);
                        });
}
