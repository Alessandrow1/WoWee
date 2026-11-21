#include "ui/character_create_screen.hpp"
#include "core/logger.hpp"
#include "ui/ui_colors.hpp"
#include "ui/wol_fantasy_theme.hpp"
#include "ui/pt_br_character_names.hpp"
#include "rendering/character_preview.hpp"
#include "rendering/renderer.hpp"
#include "core/application.hpp"
#include "pipeline/asset_manager.hpp"
#include "pipeline/dbc_layout.hpp"
#include <imgui.h>
#include <algorithm>

namespace wowee {
namespace ui {

// Full WotLK race/class lists (used as defaults when no expansion constraints set)
static constexpr game::Race kAllRaces[] = {
    // Alliance
    game::Race::HUMAN, game::Race::DWARF, game::Race::NIGHT_ELF,
    game::Race::GNOME, game::Race::DRAENEI,
    // Horde
    game::Race::ORC, game::Race::UNDEAD, game::Race::TAUREN,
    game::Race::TROLL, game::Race::BLOOD_ELF,
};
static constexpr int kAllRaceCount = 10;
static constexpr int kAllianceCount = 5;

static constexpr game::Class kAllClasses[] = {
    game::Class::WARRIOR, game::Class::PALADIN, game::Class::HUNTER,
    game::Class::ROGUE, game::Class::PRIEST, game::Class::DEATH_KNIGHT,
    game::Class::SHAMAN, game::Class::MAGE, game::Class::WARLOCK,
    game::Class::DRUID,
};

namespace {

uint8_t selectedAppearanceId(const std::vector<uint8_t>& ids, int index) {
    if (!ids.empty() && index >= 0 && index < static_cast<int>(ids.size())) {
        return ids[static_cast<size_t>(index)];
    }
    return static_cast<uint8_t>(std::max(index, 0));
}

void sortUnique(std::vector<uint8_t>& ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

// Last resort when the DBC scan found nothing for this race and sex. The
// numbers it invents are not backed by any CharSections row, so a character
// created from them has an appearance the client cannot draw - a face picked
// this way never resolves to a texture and the head renders bare. Keeping the
// fallback (an empty slider is worse than a wrong one) but saying so, because
// silently offering unrenderable choices is how a character ends up broken for
// good at the one moment it cannot be changed.
void useFallbackRange(const char* what, std::vector<uint8_t>& ids, int maxId) {
    if (!ids.empty()) return;
    core::Logger::getInstance().warning(
        "Character creation: no DBC entries for ", what,
        " - offering an unverified 0..", maxId,
        " range; choices may not render");
    ids.reserve(static_cast<size_t>(maxId + 1));
    for (int id = 0; id <= maxId; ++id) {
        ids.push_back(static_cast<uint8_t>(id));
    }
}

} // namespace


CharacterCreateScreen::CharacterCreateScreen() {
    reset();
}

CharacterCreateScreen::~CharacterCreateScreen() = default;

void CharacterCreateScreen::setExpansionConstraints(
        const std::vector<uint32_t>& races, const std::vector<uint32_t>& classes) {
    // Build filtered race list: alliance first, then horde
    availableRaces_.clear();
    expansionClasses_.clear();

    if (!races.empty()) {
        // Alliance races in display order
        for (auto r : std::initializer_list<game::Race>{
                game::Race::HUMAN, game::Race::DWARF, game::Race::NIGHT_ELF,
                game::Race::GNOME, game::Race::DRAENEI}) {
            if (std::find(races.begin(), races.end(), static_cast<uint32_t>(r)) != races.end()) {
                availableRaces_.push_back(r);
            }
        }
        allianceRaceCount_ = static_cast<int>(availableRaces_.size());

        // Horde races in display order
        for (auto r : std::initializer_list<game::Race>{
                game::Race::ORC, game::Race::UNDEAD, game::Race::TAUREN,
                game::Race::TROLL, game::Race::BLOOD_ELF}) {
            if (std::find(races.begin(), races.end(), static_cast<uint32_t>(r)) != races.end()) {
                availableRaces_.push_back(r);
            }
        }
    }

    if (!classes.empty()) {
        for (auto cls : kAllClasses) {
            if (std::find(classes.begin(), classes.end(), static_cast<uint32_t>(cls)) != classes.end()) {
                expansionClasses_.push_back(cls);
            }
        }
    }

    // If no constraints provided, fall back to WotLK defaults
    if (availableRaces_.empty()) {
        availableRaces_.assign(kAllRaces, kAllRaces + kAllRaceCount);
        allianceRaceCount_ = kAllianceCount;
    }

    raceIndex = 0;
    classIndex = 0;
    updateAvailableClasses();
}

void CharacterCreateScreen::reset() {
    name_.clear();
    raceIndex = 0;
    classIndex = 0;
    genderIndex = 0;
    bodyTypeIndex = 0;
    skin = 0;
    face = 0;
    hairStyle = 0;
    hairColor = 0;
    facialHair = 0;
    // The same limits game::getMax* answers, asked rather than repeated: this
    // is the fallback before the DBC scan has run, and the two have to agree
    // or the sliders offer a face the scan will not produce.
    const game::Race race = availableRaces_.empty()
        ? game::Race::HUMAN : availableRaces_[0];
    const game::Gender gender = game::Gender::MALE;
    maxSkin = game::getMaxSkin(race, gender);
    maxFace = game::getMaxFace(race, gender);
    maxHairStyle = game::getMaxHairStyle(race, gender);
    maxHairColor = game::getMaxHairColor(race, gender);
    maxFacialHair = game::getMaxFacialFeature(race, gender);
    statusMessage.clear();
    statusIsError = false;
    createTimer_ = -1.0f;
    skinIds_.clear();
    faceIds_.clear();
    hairStyleIds_.clear();
    hairColorIds_.clear();
    facialHairIds_.clear();

    // Populate default races if not yet set by setExpansionConstraints
    if (availableRaces_.empty()) {
        availableRaces_.assign(kAllRaces, kAllRaces + kAllRaceCount);
        allianceRaceCount_ = kAllianceCount;
    }

    updateAvailableClasses();

    // Reset preview tracking to force model reload on next render
    prevRaceIndex_ = -1;
    prevGenderIndex_ = -1;
    prevBodyTypeIndex_ = -1;
    prevSkin_ = -1;
    prevFace_ = -1;
    prevHairStyle_ = -1;
    prevHairColor_ = -1;
    prevFacialHair_ = -1;
    prevRangeRace_ = -1;
    prevRangeGender_ = -1;
    prevRangeBodyType_ = -1;
    prevRangeSkin_ = -1;
    prevRangeHairStyle_ = -1;
}

void CharacterCreateScreen::initializePreview(pipeline::AssetManager* am) {
    assetManager_ = am;
    if (!preview_) {
        preview_ = std::make_unique<rendering::CharacterPreview>();
        if (preview_->initialize(am)) {
            auto* renderer = core::Application::getInstance().getRenderer();
            if (renderer) renderer->registerPreview(preview_.get());
        }
    }
    if (preview_) preview_->resetView();
    // Force model reload
    prevRaceIndex_ = -1;
}

void CharacterCreateScreen::update(float deltaTime) {
    if (preview_) {
        preview_->update(deltaTime);
    }
    // Timeout waiting for server response
    if (createTimer_ >= 0.0f) {
        createTimer_ += deltaTime;
        if (createTimer_ > 10.0f) {
            createTimer_ = -1.0f;
            setStatus("O servidor não respondeu. Tente novamente.", true);
        }
    }
}

void CharacterCreateScreen::setStatus(const std::string& msg, bool isError) {
    statusMessage = msg;
    statusIsError = isError;
    if (isError || msg.empty()) {
        createTimer_ = -1.0f;  // Stop waiting on error/clear
    }
}

void CharacterCreateScreen::updateAvailableClasses() {
    availableClasses.clear();
    if (availableRaces_.empty() || raceIndex >= static_cast<int>(availableRaces_.size())) return;
    game::Race race = availableRaces_[raceIndex];
    for (auto cls : kAllClasses) {
        if (!game::isValidRaceClassCombo(race, cls)) continue;
        // If expansion constraints set, only allow listed classes
        if (!expansionClasses_.empty()) {
            if (std::find(expansionClasses_.begin(), expansionClasses_.end(), cls) == expansionClasses_.end())
                continue;
        }
        availableClasses.push_back(cls);
    }
    // Clamp class index
    if (classIndex >= static_cast<int>(availableClasses.size())) {
        classIndex = 0;
    }
}

void CharacterCreateScreen::updatePreviewIfNeeded() {
    if (!preview_) return;

    bool changed = (raceIndex != prevRaceIndex_ ||
                    genderIndex != prevGenderIndex_ ||
                    bodyTypeIndex != prevBodyTypeIndex_ ||
                    skin != prevSkin_ ||
                    face != prevFace_ ||
                    hairStyle != prevHairStyle_ ||
                    hairColor != prevHairColor_ ||
                    facialHair != prevFacialHair_);

    if (changed) {
        bool useFemaleModel = (genderIndex == 2 && bodyTypeIndex == 1);  // Nonbinary + Feminine
        preview_->loadCharacter(
            availableRaces_[raceIndex],
            static_cast<game::Gender>(genderIndex),
            selectedAppearanceId(skinIds_, skin),
            selectedAppearanceId(faceIds_, face),
            selectedAppearanceId(hairStyleIds_, hairStyle),
            selectedAppearanceId(hairColorIds_, hairColor),
            selectedAppearanceId(facialHairIds_, facialHair),
            useFemaleModel);

        prevRaceIndex_ = raceIndex;
        prevGenderIndex_ = genderIndex;
        prevBodyTypeIndex_ = bodyTypeIndex;
        prevSkin_ = skin;
        prevFace_ = face;
        prevHairStyle_ = hairStyle;
        prevHairColor_ = hairColor;
        prevFacialHair_ = facialHair;
    }
}

void CharacterCreateScreen::updateAppearanceRanges() {
    if (raceIndex == prevRangeRace_ &&
        genderIndex == prevRangeGender_ &&
        bodyTypeIndex == prevRangeBodyType_ &&
        skin == prevRangeSkin_ &&
        hairStyle == prevRangeHairStyle_) {
        return;
    }

    // The fallback ranges, before the DBC scan narrows them to what this race
    // and sex actually have art for.
    const game::Race race = raceIndex < static_cast<int>(availableRaces_.size())
        ? availableRaces_[raceIndex] : game::Race::HUMAN;
    const game::Gender gender =
        genderIndex == 0 ? game::Gender::MALE : game::Gender::FEMALE;
    maxSkin = game::getMaxSkin(race, gender);
    maxFace = game::getMaxFace(race, gender);
    maxHairStyle = game::getMaxHairStyle(race, gender);
    maxHairColor = game::getMaxHairColor(race, gender);
    maxFacialHair = game::getMaxFacialFeature(race, gender);
    skinIds_.clear();
    faceIds_.clear();
    hairStyleIds_.clear();
    hairColorIds_.clear();
    facialHairIds_.clear();

    if (!assetManager_ || availableRaces_.empty()) return;
    auto dbc = assetManager_->loadDBC("CharSections.dbc");
    if (!dbc) return;

    uint32_t targetRaceId = static_cast<uint32_t>(availableRaces_[raceIndex]);
    const bool useFemaleModel = genderIndex == 1 || (genderIndex == 2 && bodyTypeIndex == 1);
    uint32_t targetSexId = useFemaleModel ? 1u : 0u;

    const auto* csL = pipeline::getActiveDBCLayout() ? pipeline::getActiveDBCLayout()->getLayout("CharSections") : nullptr;
    auto csF = pipeline::detectCharSectionsFields(dbc.get(), csL);
    for (uint32_t r = 0; r < dbc->getRecordCount(); r++) {
        uint32_t raceId = dbc->getUInt32(r, csF.raceId);
        uint32_t sexId = dbc->getUInt32(r, csF.sexId);
        if (raceId != targetRaceId || sexId != targetSexId) continue;

        uint32_t baseSection = dbc->getUInt32(r, csF.baseSection);
        uint32_t variationIndex = dbc->getUInt32(r, csF.variationIndex);
        uint32_t colorIndex = dbc->getUInt32(r, csF.colorIndex);

        if (baseSection == 0 && variationIndex == 0 && colorIndex <= 255) {
            skinIds_.push_back(static_cast<uint8_t>(colorIndex));
        } else if (baseSection == 3 && variationIndex <= 255) {
            hairStyleIds_.push_back(static_cast<uint8_t>(variationIndex));
        }
    }

    sortUnique(skinIds_);
    sortUnique(hairStyleIds_);
    useFallbackRange("skin", skinIds_, maxSkin);
    useFallbackRange("hair style", hairStyleIds_, maxHairStyle);
    maxSkin = static_cast<int>(skinIds_.size()) - 1;
    maxHairStyle = static_cast<int>(hairStyleIds_.size()) - 1;
    skin = std::clamp(skin, 0, maxSkin);
    hairStyle = std::clamp(hairStyle, 0, maxHairStyle);

    const uint8_t skinId = selectedAppearanceId(skinIds_, skin);
    const uint8_t hairStyleId = selectedAppearanceId(hairStyleIds_, hairStyle);

    for (uint32_t r = 0; r < dbc->getRecordCount(); r++) {
        uint32_t raceId = dbc->getUInt32(r, csF.raceId);
        uint32_t sexId = dbc->getUInt32(r, csF.sexId);
        if (raceId != targetRaceId || sexId != targetSexId) continue;

        uint32_t baseSection = dbc->getUInt32(r, csF.baseSection);
        uint32_t variationIndex = dbc->getUInt32(r, csF.variationIndex);
        uint32_t colorIndex = dbc->getUInt32(r, csF.colorIndex);

        if (baseSection == 1 && colorIndex == skinId && variationIndex <= 255) {
            faceIds_.push_back(static_cast<uint8_t>(variationIndex));
        } else if (baseSection == 3 && variationIndex == hairStyleId) {
            if (colorIndex <= 255) {
                hairColorIds_.push_back(static_cast<uint8_t>(colorIndex));
            }
        }
    }

    sortUnique(faceIds_);
    sortUnique(hairColorIds_);
    useFallbackRange("face", faceIds_, maxFace);
    useFallbackRange("hair colour", hairColorIds_, maxHairColor);
    maxFace = static_cast<int>(faceIds_.size()) - 1;
    maxHairColor = static_cast<int>(hairColorIds_.size()) - 1;
    face = std::clamp(face, 0, maxFace);
    hairColor = std::clamp(hairColor, 0, maxHairColor);

    auto facialDbc = assetManager_->loadDBC("CharacterFacialHairStyles.dbc");
    const auto* fhL = pipeline::getActiveDBCLayout() ? pipeline::getActiveDBCLayout()->getLayout("CharacterFacialHairStyles") : nullptr;
    if (facialDbc) {
        for (uint32_t r = 0; r < facialDbc->getRecordCount(); r++) {
            uint32_t raceId = facialDbc->getUInt32(r, fhL ? (*fhL)["RaceID"] : 0);
            uint32_t sexId = facialDbc->getUInt32(r, fhL ? (*fhL)["SexID"] : 1);
            if (raceId != targetRaceId || sexId != targetSexId) continue;
            uint32_t variation = facialDbc->getUInt32(r, fhL ? (*fhL)["Variation"] : 2);
            if (variation <= 255) {
                facialHairIds_.push_back(static_cast<uint8_t>(variation));
            }
        }
    }
    sortUnique(facialHairIds_);
    useFallbackRange("facial hair", facialHairIds_, targetSexId == 1 ? 0 : maxFacialHair);
    maxFacialHair = static_cast<int>(facialHairIds_.size()) - 1;
    facialHair = std::clamp(facialHair, 0, maxFacialHair);

    prevRangeRace_ = raceIndex;
    prevRangeGender_ = genderIndex;
    prevRangeBodyType_ = bodyTypeIndex;
    prevRangeSkin_ = skin;
    prevRangeHairStyle_ = hairStyle;
}

namespace {

// Written in units and drawn at a scale taken from the window, the same way
// the login card is. See paper_ui.hpp.
constexpr float kSheetWidth  = 1160.0f;
constexpr float kSheetHeight = 740.0f;
constexpr float kPad         = 34.0f;
constexpr float kTitleSize   = 40.0f;
constexpr float kLabelSize   = 14.0f;
constexpr float kBodySize    = 15.0f;
constexpr float kSmallSize   = 13.0f;
constexpr float kFieldHeight = 38.0f;
constexpr float kChipHeight  = 30.0f;
constexpr float kButtonH     = 44.0f;
constexpr float kColumnGap   = 28.0f;

/// Alliance blue and Horde red, before the paper takes the glare off them.
constexpr ImVec4 kAllianceColour{0.30f, 0.50f, 1.00f, 1.0f};
constexpr ImVec4 kHordeColour{0.85f, 0.25f, 0.22f, 1.0f};

} // namespace

void CharacterCreateScreen::render(game::GameHandler& /*gameHandler*/) {
    // Resolve valid DBC option IDs before loading the preview. Doing this after
    // loadCharacter left race/style changes using the previous option mapping.
    updateAppearanceRanges();

    // Render the preview to FBO before the ImGui frame
    if (preview_) {
        updatePreviewIfNeeded();
        preview_->render();
        preview_->requestComposite();
    }
    const bool hasPreview = (preview_ && preview_->getTextureId() != nullptr &&
                             preview_->getWidth() > 0 && preview_->getHeight() > 0);

    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    // This sheet carries more at once than the others and none of it can be
    // scrolled to, so it shrinks to fit the window rather than running off the
    // bottom of it.
    float scale = std::clamp(std::min(screen.x / 1280.0f, screen.y / 760.0f), 0.62f, 2.6f);
    scale = std::min(scale, (screen.y - 32.0f) / kSheetHeight);
    scale = std::min(scale, (screen.x - 32.0f) / kSheetWidth);
    scale = std::max(scale, 0.34f);

    ui_.begin(ImGui::GetIO().DeltaTime, scale);
    applyWolFantasyTheme(ui_);
    drawWolScreenFrame(screen, scale);
    ui_.setLayer(PaperLayer::Page);

    const PaperTheme& theme = ui_.theme();
    const auto px = [this](float units) { return ui_.px(units); };

    const float sheetW = px(kSheetWidth);
    const float sheetH = px(kSheetHeight);
    const ImVec2 a((screen.x - sheetW) * 0.5f, (screen.y - sheetH) * 0.5f);
    const ImVec2 b(a.x + sheetW, a.y + sheetH);
    ui_.sheet(a, b, /*taped=*/false);
    ui_.claimMouse(a, b);

    const float titleSize = px(kTitleSize);
    const float bodySize = px(kBodySize);
    const float smallSize = px(kSmallSize);
    const float labelSize = px(kLabelSize);
    const float labelRow = ui_.lineHeight(labelSize) + px(3);
    const float chipH = px(kChipHeight);
    const float chipGap = px(6);

    Column page{a.x + px(kPad), b.x - px(kPad), a.y + px(kPad)};

    // ---- heading ----------------------------------------------------------
    ui_.text(page.at(), "FORJE SEU HERÓI", titleSize, theme.ink, /*titleFace=*/true);
    {
        const float w = ui_.textWidth("FORJE SEU HERÓI", titleSize, true);
        const float y = page.y + ui_.inkHeight(titleSize, true);
        ui_.squiggle(ImVec2(page.x0, y), ImVec2(page.x0 + w * 1.04f, y), theme.crayonRed,
                     px(2.0f), 0x7731u);
    }
    ui_.textRight(page.x1, page.y + titleSize * 0.55f, "Escolha sua origem e molde sua lenda", smallSize,
                  theme.pencil);
    page.gap(ui_.inkHeight(titleSize, true) + px(18));

    // ---- three columns ----------------------------------------------------
    const float footerH = px(kButtonH) + ui_.lineHeight(smallSize) + px(20);
    const float bodyTop = page.y;
    const float bodyBottom = b.y - px(kPad) - footerH;
    const float columnsW = page.width() - px(kColumnGap) * 2.0f;
    const float previewW = columnsW * 0.37f;
    const float identityW = columnsW * 0.34f;
    const float appearanceW = columnsW - previewW - identityW;
    const float identityX = page.x0 + previewW + px(kColumnGap);
    const float appearanceX = identityX + identityW + px(kColumnGap);

    {
        const float dividers[] = {identityX - px(kColumnGap) * 0.5f,
                                  appearanceX - px(kColumnGap) * 0.5f};
        for (uint32_t i = 0; i < 2; ++i) {
            ui_.rule(ImVec2(dividers[i], bodyTop), ImVec2(dividers[i], bodyBottom),
                     paperFade(theme.pencil, 0.5f), px(1.0f), 0x3355u + i);
        }
    }

    // ---- the picture ------------------------------------------------------
    if (hasPreview) {
        const float aspect = static_cast<float>(preview_->getHeight()) /
                             static_cast<float>(preview_->getWidth());
        const float mat = px(9);
        const float hintH = ui_.lineHeight(smallSize) + px(8);
        float imgW = previewW - mat * 2.0f;
        float imgH = imgW * aspect;
        const float roomH = (bodyBottom - bodyTop) - mat * 2.0f - hintH;
        if (imgH > roomH) {
            imgH = std::max(roomH, px(120));
            imgW = imgH / aspect;
        }
        const float imgX = page.x0 + (previewW - imgW) * 0.5f;
        const ImVec2 imgA(imgX, bodyTop + mat);
        const ImVec2 imgB(imgX + imgW, bodyTop + mat + imgH);
        ui_.image(reinterpret_cast<ImTextureID>(preview_->getTextureId()), imgA, imgB);

        // Mouse drag rotation and hover-only wheel zoom on the preview image.
        if (ui_.hover(imgA, imgB)) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                preview_->rotate(ImGui::GetIO().MouseDelta.x * 0.2f);
            }
            if (ImGui::GetIO().MouseWheel != 0.0f) {
                preview_->zoom(ImGui::GetIO().MouseWheel);
            }
        }
        ui_.textCentered(page.x0 + previewW * 0.5f, imgB.y + mat + px(4),
                         "Arraste para girar  -  role para ampliar", smallSize, theme.pencil);
    } else {
        ui_.textCentered(page.x0 + previewW * 0.5f, bodyTop + px(20),
                         "Prévia indisponível.", bodySize, theme.pencil);
    }

    // ---- who they are -----------------------------------------------------
    Column who{identityX, identityX + identityW, bodyTop};

    // Rows of pills that wrap when they run out of line.
    float flowX = who.x0;
    float flowY = who.y;
    const auto beginFlow = [&]() {
        flowX = who.x0;
        flowY = who.y;
    };
    const auto pill = [&](const char* label, bool selected, ImVec4 accent) {
        const float w = ui_.chipWidth(label, chipH);
        if (flowX + w > who.x1 && flowX > who.x0) {
            flowX = who.x0;
            flowY += chipH + chipGap;
        }
        const bool hit = ui_.chip(label, ImVec2(flowX, flowY), ImVec2(flowX + w, flowY + chipH),
                                  label, selected, ui_.onPaper(accent));
        flowX += w + chipGap;
        return hit;
    };
    const auto endFlow = [&](float trailingGap) {
        who.y = flowY + chipH + trailingGap;
    };

    ui_.text(who.at(), "Nome", labelSize, theme.inkSoft);
    ui_.textRight(who.x1, who.y + (labelSize - smallSize) * 0.5f, "Até 12 letras", smallSize,
                  theme.pencil);
    who.gap(labelRow);
    {
        const auto [fa, fb] = who.row(px(kFieldHeight));
        PaperUI::FieldOpts opts;
        opts.placeholder = "Nome do personagem";
        const PaperUI::FieldResult r = ui_.field("name", fa, fb, name_, opts);
        if (r.changed && statusIsError) setStatus("", false);
        who.gap(px(14));
    }

    const int raceCount = static_cast<int>(availableRaces_.size());
    const auto pickRace = [&](int i) {
        if (raceIndex == i) return;
        raceIndex = i;
        classIndex = 0;
        skin = face = hairStyle = hairColor = facialHair = 0;
        updateAvailableClasses();
    };

    if (allianceRaceCount_ > 0) {
        ui_.text(who.at(), "Aliança", labelSize, ui_.onPaper(kAllianceColour));
        who.gap(labelRow);
        beginFlow();
        for (int i = 0; i < allianceRaceCount_; ++i) {
            if (pill(ptbr::raceName(availableRaces_[static_cast<size_t>(i)]), raceIndex == i,
                     kAllianceColour)) {
                pickRace(i);
            }
        }
        endFlow(px(12));
    }
    if (allianceRaceCount_ < raceCount) {
        ui_.text(who.at(), "Horda", labelSize, ui_.onPaper(kHordeColour));
        who.gap(labelRow);
        beginFlow();
        for (int i = allianceRaceCount_; i < raceCount; ++i) {
            if (pill(ptbr::raceName(availableRaces_[static_cast<size_t>(i)]), raceIndex == i,
                     kHordeColour)) {
                pickRace(i);
            }
        }
        endFlow(px(14));
    }

    ui_.text(who.at(), "Classe", labelSize, theme.inkSoft);
    who.gap(labelRow);
    if (availableClasses.empty()) {
        ui_.text(who.at(), "Nenhuma classe disponível para esta raça.", smallSize, theme.crayonRed);
        who.gap(ui_.lineHeight(smallSize) + px(14));
    } else {
        beginFlow();
        for (int i = 0; i < static_cast<int>(availableClasses.size()); ++i) {
            const auto cls = availableClasses[static_cast<size_t>(i)];
            if (pill(ptbr::className(cls), classIndex == i,
                     ui::getClassColor(static_cast<uint8_t>(cls)))) {
                classIndex = i;
            }
        }
        endFlow(px(14));
    }

    ui_.text(who.at(), "Gênero", labelSize, theme.inkSoft);
    who.gap(labelRow);
    beginFlow();
    if (pill("Masculino", genderIndex == 0, kAllianceColour)) genderIndex = 0;
    if (pill("Feminino", genderIndex == 1, kHordeColour)) genderIndex = 1;
    endFlow(px(12));

    // TODO(server): Re-enable the nonbinary option and the body-type controls
    // once character creation accepts gender=2 plus the selected model body.
    // The renderer/data plumbing remains in place so server support can enable
    // it: it would be one more pill here and a pair below it for the body.

    // ---- what they look like ----------------------------------------------
    //
    // Race and gender above may have changed this frame; refresh their option
    // lists before presenting customization controls.
    updateAppearanceRanges();
    const game::Gender currentGender = static_cast<game::Gender>(genderIndex);

    Column look{appearanceX, appearanceX + appearanceW, bodyTop};
    ui_.text(look.at(), "PERSONALIZAÇÃO", px(22), theme.ink, /*titleFace=*/true);
    look.gap(px(30));

    // No numeric sliders: the player cycles real DBC-backed styles with
    // simple previous/next buttons while the 3D preview updates immediately.
    // Labels and option count are intentionally separate from the DBC IDs.
    const auto appearancePicker = [&](const char* id, const char* label, int* value, int maxVal) {
        ui_.text(look.at(), label, labelSize, theme.inkSoft);
        look.gap(labelRow);
        const auto [sa, sb] = look.row(px(36));
        const float arrowW = px(43);
        const bool canChoose = maxVal > 0;
        const std::string previousId = std::string(id) + ".previous";
        const std::string nextId = std::string(id) + ".next";
        if (ui_.button(previousId.c_str(), sa,
                       ImVec2(sa.x + arrowW, sb.y), "<",
                       PaperUI::ButtonKind::Quiet, canChoose)) {
            *value = (*value <= 0) ? maxVal : (*value - 1);
        }
        if (ui_.button(nextId.c_str(), ImVec2(sb.x - arrowW, sa.y), sb, ">",
                       PaperUI::ButtonKind::Quiet, canChoose)) {
            *value = (*value >= maxVal) ? 0 : (*value + 1);
        }
        if (canChoose) {
            const std::string count = std::to_string(*value + 1) + " / " +
                                      std::to_string(maxVal + 1);
            ui_.textCentered((sa.x + sb.x) * 0.5f,
                             sa.y + px(7), count.c_str(), bodySize, theme.ink);
        } else {
            ui_.textCentered((sa.x + sb.x) * 0.5f,
                             sa.y + px(8), "Única opção", smallSize, theme.pencil);
        }
        look.gap(px(9));
    };

    // Colour swatches are visual navigation, not guesses at the server's
    // CharSections IDs. The actual texture always comes from the 3D preview.
    // Provide a short, scrollable (by pages) grid to avoid overflowing the
    // fixed-height creation sheet on races with many hair colour options.
    const auto colourPalette = [&](const char* id, const char* label,
                                   int* value, int maxVal, bool forHair) {
        ui_.text(look.at(), label, labelSize, theme.inkSoft);
        look.gap(labelRow);
        const int options = std::max(1, maxVal + 1);
        constexpr int kColumns = 7;
        constexpr int kVisible = 14;
        const int page = *value / kVisible;
        const int first = page * kVisible;
        const int last = std::min(options, first + kVisible);
        const float swatch = px(26);
        const float spacing = px(7);
        const ImVec2 gridOrigin = look.at();
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        // Representative colours only: a DBC variation index is not an RGB
        // value. Never use the swatch colour as the actual character option.
        static constexpr ImU32 kSkinColours[] = {
            IM_COL32(249, 222, 190, 255), IM_COL32(232, 190, 151, 255),
            IM_COL32(204, 153, 114, 255), IM_COL32(161, 112, 82, 255),
            IM_COL32(119, 80, 63, 255), IM_COL32(82, 60, 54, 255),
            IM_COL32(174, 185, 178, 255), IM_COL32(121, 151, 168, 255),
            IM_COL32(119, 142, 105, 255), IM_COL32(163, 121, 149, 255)
        };
        static constexpr ImU32 kHairColours[] = {
            IM_COL32(42, 32, 32, 255), IM_COL32(83, 52, 34, 255),
            IM_COL32(131, 77, 46, 255), IM_COL32(176, 114, 62, 255),
            IM_COL32(226, 183, 103, 255), IM_COL32(243, 219, 166, 255),
            IM_COL32(195, 195, 188, 255), IM_COL32(122, 133, 141, 255),
            IM_COL32(154, 53, 44, 255), IM_COL32(225, 229, 231, 255)
        };
        const int paletteSize = 10;

        for (int i = first; i < last; ++i) {
            const int offset = i - first;
            const float x = gridOrigin.x + (offset % kColumns) * (swatch + spacing);
            const float y = gridOrigin.y + (offset / kColumns) * (swatch + spacing);
            const ImVec2 a(x, y);
            const ImVec2 b(x + swatch, y + swatch);
            const bool hovered = ui_.hover(a, b);
            if (hovered) {
                ImGui::BeginTooltip();
                ImGui::Text("Opção %d de %d", i + 1, options);
                ImGui::TextUnformatted("Amostra ilustrativa; confira a cor no personagem 3D.");
                ImGui::EndTooltip();
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    *value = i;
                }
            }
            const ImU32 colour = forHair
                ? kHairColours[i % paletteSize] : kSkinColours[i % paletteSize];
            draw->AddRectFilled(a, b, colour, px(4));
            draw->AddRect(a, b, i == *value ? theme.crayonRedLit : theme.paperEdge,
                          px(4), 0, i == *value ? px(3) : px(1));
            if (i == *value) {
                draw->AddRect(ImVec2(a.x - px(3), a.y - px(3)),
                              ImVec2(b.x + px(3), b.y + px(3)),
                              theme.ink, px(5), 0, px(1.5f));
            }
        }
        look.gap(px((last - first > kColumns) ? 72 : 38));

        // Browse other options without adding more rows to the panel.
        if (options > kVisible) {
            const auto [a, b] = look.row(px(28));
            const float bw = px(40);
            const std::string backId = std::string(id) + ".pageback";
            const std::string nextId = std::string(id) + ".pagenext";
            if (ui_.button(backId.c_str(), a, ImVec2(a.x + bw, b.y), "<",
                           PaperUI::ButtonKind::Quiet, first > 0)) {
                *value = std::max(0, first - kVisible);
            }
            if (ui_.button(nextId.c_str(), ImVec2(b.x - bw, a.y), b, ">",
                           PaperUI::ButtonKind::Quiet, last < options)) {
                *value = last;
            }
            const std::string counter = std::to_string(*value + 1) +
                                        " / " + std::to_string(options);
            ui_.textCentered((a.x + b.x) * 0.5f, a.y + px(5),
                             counter.c_str(), smallSize, theme.inkSoft);
            look.gap(px(4));
        }
        look.gap(px(7));
    };

    colourPalette("look.skin", "Tom de pele", &skin, maxSkin, false);
    appearancePicker("look.face", "Rosto", &face, maxFace);
    appearancePicker("look.hairstyle", "Cabelo", &hairStyle, maxHairStyle);
    colourPalette("look.haircolor", "Cor do cabelo", &hairColor, maxHairColor, true);
    const bool humanMale = !availableRaces_.empty() &&
        availableRaces_[static_cast<size_t>(raceIndex)] == game::Race::HUMAN &&
        genderIndex == 0;
    appearancePicker("look.facialhair", humanMale ? "Barba e bigode" : "Detalhes faciais",
                     &facialHair, maxFacialHair);

    // Skin and hairstyle choose the valid face/color subsets. Refresh now so a
    // Create click in this same frame sends IDs from the newly selected subset.
    updateAppearanceRanges();

    // ---- footer -----------------------------------------------------------
    page.y = bodyBottom + px(10);
    ui_.rule(ImVec2(page.x0, page.y), ImVec2(page.x1, page.y), paperFade(theme.pencil, 0.55f),
             px(1.0f), 0x5512u);
    page.gap(px(8));

    if (!statusMessage.empty()) {
        ui_.text(page.at(), statusMessage.c_str(), smallSize,
                 statusIsError ? theme.crayonRed : theme.crayonGreen);
    } else {
        ui_.text(page.at(), "Escolha uma raça e uma classe disponível. O nome pode ter até 12 "
                            "letras.", smallSize, theme.pencil);
    }
    page.gap(ui_.lineHeight(smallSize) + px(8));

    const auto submit = [&]() {
        std::string name = name_.text();
        // Trim whitespace
        const size_t start = name.find_first_not_of(" \t\r\n");
        const size_t end = name.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) {
            name.clear();
        } else {
            name = name.substr(start, end - start + 1);
        }
        if (name.empty()) {
            setStatus("Dê um nome ao personagem primeiro.", true);
            ui_.focus("name");
            return;
        }
        if (availableClasses.empty()) {
            setStatus("Nenhuma classe válida para esta raça.", true);
            return;
        }
        setStatus("Criando personagem...", false);
        createTimer_ = 0.0f;
        game::CharCreateData data;
        data.name = name;
        data.race = availableRaces_[static_cast<size_t>(raceIndex)];
        data.characterClass = availableClasses[static_cast<size_t>(classIndex)];
        data.gender = currentGender;
        data.useFemaleModel = (genderIndex == 2 && bodyTypeIndex == 1);  // Nonbinary + Feminine
        data.skin = selectedAppearanceId(skinIds_, skin);
        data.face = selectedAppearanceId(faceIds_, face);
        data.hairStyle = selectedAppearanceId(hairStyleIds_, hairStyle);
        data.hairColor = selectedAppearanceId(hairColorIds_, hairColor);
        data.facialHair = selectedAppearanceId(facialHairIds_, facialHair);
        if (onCreate) {
            onCreate(data);
        }
    };

    {
        const float y = page.y;
        const float h = px(kButtonH);
        if (ui_.button("create.back", ImVec2(page.x0, y), ImVec2(page.x0 + px(110), y + h),
                       "Voltar")) {
            if (onCancel) onCancel();
        }
        const float createW = px(210);
        const bool waiting = (createTimer_ >= 0.0f);
        if (ui_.button("create.go", ImVec2(page.x1 - createW, y), ImVec2(page.x1, y + h),
                       waiting ? "Criando..." : "CRIAR PERSONAGEM",
                       PaperUI::ButtonKind::Primary, !waiting)) {
            submit();
        }
    }

    // Enter in the name box, or with the keyboard nowhere, is the same as
    // pressing the button.
    if (createTimer_ < 0.0f &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
         ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        submit();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (!ui_.popupOpen()) {
            if (ui_.wantsTextInput()) {
                ui_.clearFocus();
            } else if (onCancel) {
                onCancel();
            }
        }
    }

    ui_.end();
}

} // namespace ui
} // namespace wowee
