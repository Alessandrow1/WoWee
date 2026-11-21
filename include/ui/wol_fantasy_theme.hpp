#pragma once
// World of Legends: dark-fantasy identity for the authentication and character
// screens. Keep this opt-in: the in-game HUD and settings retain their theme.
#include "ui/paper_ui.hpp"

namespace wowee::ui {

inline void applyWolFantasyTheme(PaperUI& ui) {
    PaperTheme& t = ui.theme();
    t.fantasyMode = true;
    // Midnight blue surfaces, soft gold typography, restrained teal highlights.
    t.paperTop     = IM_COL32(15, 27, 46, 250);
    t.paperBottom  = IM_COL32(8, 16, 29, 255);
    t.paperEdge    = IM_COL32(174, 137, 75, 255);
    t.shadow       = IM_COL32(0, 0, 0, 205);
    t.ink          = IM_COL32(242, 219, 169, 255);
    t.inkSoft      = IM_COL32(205, 199, 183, 255);
    t.pencil       = IM_COL32(156, 176, 193, 255);
    t.fieldFill    = IM_COL32(29, 45, 67, 235);
    t.crayonRed    = IM_COL32(195, 151, 73, 255);
    t.crayonRedLit = IM_COL32(227, 190, 111, 255);
    t.crayonRedDim = IM_COL32(119, 87, 42, 255);
    t.crayonBlue   = IM_COL32(84, 162, 219, 255);
    t.crayonGreen  = IM_COL32(73, 193, 153, 255);
    t.highlighter  = IM_COL32(74, 111, 142, 155);
    t.onCrayon     = IM_COL32(12, 22, 36, 255);
    // Do not wash out the character preview: keep original 3D colours.
    t.photoMat     = IM_COL32(14, 25, 42, 255);
    t.photoFade    = 0.0f;
    t.photoTint    = IM_COL32(255, 255, 255, 255);
}

inline void drawWolScreenFrame(ImVec2 screen, float scale) {
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    // Subtle vignetting. This layer remains behind every interactive control.
    bg->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(3, 9, 18, 74));
    const float margin = 22.0f * scale;
    const float y = 18.0f * scale;
    bg->AddLine(ImVec2(margin, y), ImVec2(screen.x - margin, y),
                IM_COL32(194, 152, 78, 155), 1.4f * scale);
    bg->AddLine(ImVec2(margin, screen.y - y), ImVec2(screen.x - margin, screen.y - y),
                IM_COL32(194, 152, 78, 110), 1.2f * scale);
}

} // namespace wowee::ui
