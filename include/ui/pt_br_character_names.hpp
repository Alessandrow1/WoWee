#pragma once

#include "game/character.hpp"

// UI-only display names: wire/protocol race, class and gender identifiers
// must stay unchanged. Keep server and gameplay lookups in their own layer.
namespace wowee::ui::ptbr {

inline const char* raceName(game::Race race) {
    switch (race) {
        case game::Race::HUMAN: return "Humano";
        case game::Race::ORC: return "Orc";
        case game::Race::DWARF: return "Anão";
        case game::Race::NIGHT_ELF: return "Elfo Noturno";
        case game::Race::UNDEAD: return "Morto-vivo";
        case game::Race::TAUREN: return "Tauren";
        case game::Race::GNOME: return "Gnomo";
        case game::Race::TROLL: return "Troll";
        case game::Race::GOBLIN: return "Goblin";
        case game::Race::BLOOD_ELF: return "Elfo Sangrento";
        case game::Race::DRAENEI: return "Draenei";
    }
    return game::getRaceName(race);
}

inline const char* className(game::Class cls) {
    switch (cls) {
        case game::Class::WARRIOR: return "Guerreiro";
        case game::Class::PALADIN: return "Paladino";
        case game::Class::HUNTER: return "Caçador";
        case game::Class::ROGUE: return "Ladino";
        case game::Class::PRIEST: return "Sacerdote";
        case game::Class::DEATH_KNIGHT: return "Cav. da Morte";
        case game::Class::SHAMAN: return "Xamã";
        case game::Class::MAGE: return "Mago";
        case game::Class::WARLOCK: return "Bruxo";
        case game::Class::DRUID: return "Druida";
    }
    return game::getClassName(cls);
}

inline const char* genderName(game::Gender gender) {
    switch (gender) {
        case game::Gender::MALE: return "Masculino";
        case game::Gender::FEMALE: return "Feminino";
        case game::Gender::NONBINARY: return "Não binário";
    }
    return game::getGenderName(gender);
}

} // namespace wowee::ui::ptbr
