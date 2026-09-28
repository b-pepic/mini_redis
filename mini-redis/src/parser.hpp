#pragma once

#include <string>
#include <vector>

// Rastavi redak teksta na rijeci (odvojene razmacima ili tabovima).
//   "SET ime Ivan"  ->  {"SET", "ime", "Ivan"}
//   "   "           ->  {}
std::vector<std::string> parse_command(const std::string& line);

// Pretvori tekst u velika slova, da "set", "Set" i "SET" budu ista naredba.
std::string to_upper(std::string text);
