#pragma once

#include <cstdint>

namespace macos_keyboard {

enum VirtualKey : uint32_t {
	kVK_F17 = 0x40,
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
	NSDeleteFunctionKey = 0xF728,
	NSHomeFunctionKey = 0xF729,
	NSEndFunctionKey = 0xF72B,
	NSPageUpFunctionKey = 0xF72C,
	NSPageDownFunctionKey = 0xF72D,
	NSHelpFunctionKey = 0xF746,
};

/* Qt reports no text for these keys, and CEF treats a key event without a
 * character as a modifier change, which turns key releases into presses. */
inline char16_t FunctionKeyCharacterFromVirtualKey(uint32_t native_vkey)
{
	switch (native_vkey) {
	case kVK_UpArrow:
		return NSUpArrowFunctionKey;
	case kVK_DownArrow:
		return NSDownArrowFunctionKey;
	case kVK_LeftArrow:
		return NSLeftArrowFunctionKey;
	case kVK_RightArrow:
		return NSRightArrowFunctionKey;
	case kVK_ForwardDelete:
		return NSDeleteFunctionKey;
	case kVK_Home:
		return NSHomeFunctionKey;
	case kVK_End:
		return NSEndFunctionKey;
	case kVK_PageUp:
		return NSPageUpFunctionKey;
	case kVK_PageDown:
		return NSPageDownFunctionKey;
	case kVK_Help:
		return NSHelpFunctionKey;
	case kVK_F1:
		return NSF1FunctionKey;
	case kVK_F2:
		return NSF1FunctionKey + 1;
	case kVK_F3:
		return NSF1FunctionKey + 2;
	case kVK_F4:
		return NSF1FunctionKey + 3;
	case kVK_F5:
		return NSF1FunctionKey + 4;
	case kVK_F6:
		return NSF1FunctionKey + 5;
	case kVK_F7:
		return NSF1FunctionKey + 6;
	case kVK_F8:
		return NSF1FunctionKey + 7;
	case kVK_F9:
		return NSF1FunctionKey + 8;
	case kVK_F10:
		return NSF1FunctionKey + 9;
	case kVK_F11:
		return NSF1FunctionKey + 10;
	case kVK_F12:
		return NSF1FunctionKey + 11;
	case kVK_F13:
		return NSF1FunctionKey + 12;
	case kVK_F14:
		return NSF1FunctionKey + 13;
	case kVK_F15:
		return NSF1FunctionKey + 14;
	case kVK_F16:
		return NSF1FunctionKey + 15;
	case kVK_F17:
		return NSF1FunctionKey + 16;
	case kVK_F18:
		return NSF1FunctionKey + 17;
	case kVK_F19:
		return NSF1FunctionKey + 18;
	case kVK_F20:
		return NSF1FunctionKey + 19;
	default:
		return 0;
	}
}

} // namespace macos_keyboard
