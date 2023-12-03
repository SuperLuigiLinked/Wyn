/**
 * @file wvk.h
 *
 * @brief Utilities for interpreting Mouse-Buttons/Key-Codes.
 */

#pragma once

#ifndef WVK_H
#define WVK_H

// ================================================================================================================================
//  Type Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief A virtual code representing a button on a mouse.
 */
typedef unsigned short wyn_button_t;

/**
 * @brief A virtual code representing a key on a keyboard.
 */
typedef unsigned short wyn_keycode_t;

/**
 * @brief Platform-independent Virtual-Button identifiers.
 */
enum wyn_vb_t
{
    wyn_unknown_vb = -1,

    wyn_vb_left,
    wyn_vb_right,
    wyn_vb_middle,

    wyn_count_vb
};
typedef enum wyn_vb_t wyn_vb_t;

/**
 * @brief Platform-independent Virtual-Key identifiers.
 */
enum wyn_vk_t
{
    wyn_unknown_vk = -1,
    
    wyn_vk_0,
    wyn_vk_1,
    wyn_vk_2,
    wyn_vk_3,
    wyn_vk_4,
    wyn_vk_5,
    wyn_vk_6,
    wyn_vk_7,
    wyn_vk_8,
    wyn_vk_9,
    
    wyn_vk_A,
    wyn_vk_B,
    wyn_vk_C,
    wyn_vk_D,
    wyn_vk_E,
    wyn_vk_F,
    wyn_vk_G,
    wyn_vk_H,
    wyn_vk_I,
    wyn_vk_J,
    wyn_vk_K,
    wyn_vk_L,
    wyn_vk_M,
    wyn_vk_N,
    wyn_vk_O,
    wyn_vk_P,
    wyn_vk_Q,
    wyn_vk_R,
    wyn_vk_S,
    wyn_vk_T,
    wyn_vk_U,
    wyn_vk_V,
    wyn_vk_W,
    wyn_vk_X,
    wyn_vk_Y,
    wyn_vk_Z,
    
    wyn_vk_Left,
    wyn_vk_Right,
    wyn_vk_Up,
    wyn_vk_Down,
    
    wyn_vk_Period,
    wyn_vk_Comma,
    wyn_vk_Semicolon,
    wyn_vk_Quote,
    wyn_vk_Slash,
    wyn_vk_Backslash,
    wyn_vk_BracketL,
    wyn_vk_BracketR,
    wyn_vk_Plus,
    wyn_vk_Minus,
    wyn_vk_Accent,

    wyn_vk_Control,
    wyn_vk_Start,
    wyn_vk_Alt,
    wyn_vk_Space,
    wyn_vk_Backspace,
    wyn_vk_Delete,
    wyn_vk_Insert,
    wyn_vk_Shift,
    wyn_vk_CapsLock,
    wyn_vk_Tab,
    wyn_vk_Enter,
    wyn_vk_Escape,
    wyn_vk_Home,
    wyn_vk_End,
    wyn_vk_PageUp,
    wyn_vk_PageDown,
    
    wyn_vk_F1,
    wyn_vk_F2,
    wyn_vk_F3,
    wyn_vk_F4,
    wyn_vk_F5,
    wyn_vk_F6,
    wyn_vk_F7,
    wyn_vk_F8,
    wyn_vk_F9,
    wyn_vk_F10,
    wyn_vk_F11,
    wyn_vk_F12,

    wyn_vk_PrintScreen,
    wyn_vk_ScrollLock,
    wyn_vk_NumLock,

    wyn_vk_Numpad0,
    wyn_vk_Numpad1,
    wyn_vk_Numpad2,
    wyn_vk_Numpad3,
    wyn_vk_Numpad4,
    wyn_vk_Numpad5,
    wyn_vk_Numpad6,
    wyn_vk_Numpad7,
    wyn_vk_Numpad8,
    wyn_vk_Numpad9,
    
    wyn_vk_NumpadPlus,
    wyn_vk_NumpadMinus,
    wyn_vk_NumpadMultiply,
    wyn_vk_NumpadDivide,
    wyn_vk_NumpadPeriod,

    wyn_count_vk
};
typedef enum wyn_vk_t wyn_vk_t;

/**
 * @brief An array of Mappings from Virtual Buttons to Button Codes.
*/
typedef wyn_button_t (wyn_vb_mapping_t)[wyn_count_vb];

/**
 * @brief An array of Mappings from Virtual Keys to Key Codes.
*/
typedef wyn_keycode_t (wyn_vk_mapping_t)[wyn_count_vk];

// ================================================================================================================================
//  API Functions
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Queries the platform-specific Virtual-Button Mappings.
 * @return [non-null] Pointer to an Array of mappings.
 */
extern const wyn_vb_mapping_t* wyn_vb_mapping(void);

/**
 * @brief Queries the platform-specific Virtual-Key Mappings.
 * @return [non-null] Pointer to an Array of mappings.
 */
extern const wyn_vk_mapping_t* wyn_vk_mapping(void);

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif
