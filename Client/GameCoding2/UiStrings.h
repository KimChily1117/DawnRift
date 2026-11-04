#pragma once

namespace UiStrings
{
const string& Get(const string& key);
// Get with {0}, {1}, ... replaced by the arguments in order.
string Format(const string& key, std::initializer_list<string> args);
}
