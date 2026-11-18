#pragma once

#include <array>
#include <cstdint>
#include <utility>

namespace macos_keyboard {

enum VirtualKey : uint32_t {
	kVK_F17 = 0x40,
	kVK_ANSI_KeypadClear = 0x47,
	kVK_F18 = 0x4F,
	kVK_F19 = 0x50,
	kVK_F20 = 0x5A,
	kVK_F5 = 0x60,
	kVK_F6 = 0x61,
	kVK_F7 = 0x62,
	kVK_F3 = 0x63,
	kVK_F8 = 0x64,
	kVK_F9 = 0x65,
	kVK_F11 = 0x67,
	kVK_F13 = 0x69,
	kVK_F16 = 0x6A,
	kVK_F14 = 0x6B,
	kVK_F10 = 0x6D,
	kVK_F12 = 0x6F,
	kVK_F15 = 0x71,
	kVK_Help = 0x72,
	kVK_Home = 0x73,
	kVK_PageUp = 0x74,
	kVK_ForwardDelete = 0x75,
	kVK_F4 = 0x76,
	kVK_End = 0x77,
	kVK_F2 = 0x78,
	kVK_PageDown = 0x79,
	kVK_F1 = 0x7A,
	kVK_LeftArrow = 0x7B,
	kVK_RightArrow = 0x7C,
	kVK_DownArrow = 0x7D,
	kVK_UpArrow = 0x7E,
};

enum FunctionKeyCharacter : char16_t {
	NSUpArrowFunctionKey = 0xF700,
	NSDownArrowFunctionKey = 0xF701,
	NSLeftArrowFunctionKey = 0xF702,
	NSRightArrowFunctionKey = 0xF703,
	NSF1FunctionKey = 0xF704,
	NSF2FunctionKey = 0xF705,
	NSF3FunctionKey = 0xF706,
	NSF4FunctionKey = 0xF707,
	NSF5FunctionKey = 0xF708,
	NSF6FunctionKey = 0xF709,
	NSF7FunctionKey = 0xF70A,
	NSF8FunctionKey = 0xF70B,
	NSF9FunctionKey = 0xF70C,
	NSF10FunctionKey = 0xF70D,
	NSF11FunctionKey = 0xF70E,
	NSF12FunctionKey = 0xF70F,
	NSF13FunctionKey = 0xF710,
	NSF14FunctionKey = 0xF711,
	NSF15FunctionKey = 0xF712,
	NSF16FunctionKey = 0xF713,
	NSF17FunctionKey = 0xF714,
	NSF18FunctionKey = 0xF715,
	NSF19FunctionKey = 0xF716,
	NSF20FunctionKey = 0xF717,
	NSDeleteFunctionKey = 0xF728,
	NSHomeFunctionKey = 0xF729,
	NSEndFunctionKey = 0xF72B,
	NSPageUpFunctionKey = 0xF72C,
	NSPageDownFunctionKey = 0xF72D,
	NSClearLineFunctionKey = 0xF739,
	NSHelpFunctionKey = 0xF746,
};

constexpr std::array<std::pair<uint32_t, char16_t>, 31> function_keys{{
	{kVK_UpArrow, NSUpArrowFunctionKey},
	{kVK_DownArrow, NSDownArrowFunctionKey},
	{kVK_LeftArrow, NSLeftArrowFunctionKey},
	{kVK_RightArrow, NSRightArrowFunctionKey},
	{kVK_F1, NSF1FunctionKey},
	{kVK_F2, NSF2FunctionKey},
	{kVK_F3, NSF3FunctionKey},
	{kVK_F4, NSF4FunctionKey},
	{kVK_F5, NSF5FunctionKey},
	{kVK_F6, NSF6FunctionKey},
	{kVK_F7, NSF7FunctionKey},
	{kVK_F8, NSF8FunctionKey},
	{kVK_F9, NSF9FunctionKey},
	{kVK_F10, NSF10FunctionKey},
	{kVK_F11, NSF11FunctionKey},
	{kVK_F12, NSF12FunctionKey},
	{kVK_F13, NSF13FunctionKey},
	{kVK_F14, NSF14FunctionKey},
	{kVK_F15, NSF15FunctionKey},
	{kVK_F16, NSF16FunctionKey},
	{kVK_F17, NSF17FunctionKey},
	{kVK_F18, NSF18FunctionKey},
	{kVK_F19, NSF19FunctionKey},
	{kVK_F20, NSF20FunctionKey},
	{kVK_ForwardDelete, NSDeleteFunctionKey},
	{kVK_Home, NSHomeFunctionKey},
	{kVK_End, NSEndFunctionKey},
	{kVK_PageUp, NSPageUpFunctionKey},
	{kVK_PageDown, NSPageDownFunctionKey},
	{kVK_ANSI_KeypadClear, NSClearLineFunctionKey},
	{kVK_Help, NSHelpFunctionKey},
}};

/* Qt reports no text for these keys, and CEF treats a key event without a
 * character as a modifier change, which turns key releases into presses. */
constexpr char16_t FunctionKeyCharacterFromVirtualKey(uint32_t native_vkey)
{
	for (const auto &[vkey, character] : function_keys) {
		if (vkey == native_vkey) {
			return character;
		}
	}
	return 0;
}

} // namespace macos_keyboard
