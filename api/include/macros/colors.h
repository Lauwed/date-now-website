#pragma once

/**
 * @file colors.h
 * @brief ANSI escape code macros for terminal colours, backgrounds, and styles.
 */

/** @brief Reset all ANSI attributes. */
#define ANSI_RESET_ALL          "\x1b[0m"

/** @brief Set foreground to a 24-bit RGB colour. */
#define ANSI_COLOR_RGB(r, g, b)    "\x1b[38;2;" #r ";" #g ";" #b "m"
/** @brief Set background to a 24-bit RGB colour. */
#define ANSI_BACKGROUND_COLOR_RGB(r, g, b) "\x1b[48;2;" #r ";" #g ";" #b "m"

/** @brief Amber foreground (255, 182, 0). */
#define ANSI_COLOR_AMBER           ANSI_COLOR_RGB(255, 182, 0)
/** @brief Amber background (255, 182, 0). */
#define ANSI_BACKGROUND_AMBER        ANSI_BACKGROUND_COLOR_RGB(255, 182, 0)

#define ANSI_COLOR_BLACK        "\x1b[30m"  /**< Black foreground. */
#define ANSI_COLOR_RED          "\x1b[31m"  /**< Red foreground. */
#define ANSI_COLOR_GREEN        "\x1b[32m"  /**< Green foreground. */
#define ANSI_COLOR_YELLOW       "\x1b[33m"  /**< Yellow foreground. */
#define ANSI_COLOR_BLUE         "\x1b[34m"  /**< Blue foreground. */
#define ANSI_COLOR_MAGENTA      "\x1b[35m"  /**< Magenta foreground. */
#define ANSI_COLOR_CYAN         "\x1b[36m"  /**< Cyan foreground. */
#define ANSI_COLOR_WHITE        "\x1b[37m"  /**< White foreground. */

#define ANSI_COLOR_BRIGHT_BLACK   "\x1b[90m"   /**< Bright black (dark grey) foreground. */
#define ANSI_COLOR_BRIGHT_RED     "\x1b[91m"   /**< Bright red foreground. */
#define ANSI_COLOR_BRIGHT_GREEN   "\x1b[92m"   /**< Bright green foreground. */
#define ANSI_COLOR_BRIGHT_YELLOW  "\x1b[93m"   /**< Bright yellow foreground. */
#define ANSI_COLOR_BRIGHT_BLUE    "\x1b[94m"   /**< Bright blue foreground. */
#define ANSI_COLOR_BRIGHT_MAGENTA "\x1b[95m"   /**< Bright magenta foreground. */
#define ANSI_COLOR_BRIGHT_CYAN    "\x1b[96m"   /**< Bright cyan foreground. */
#define ANSI_COLOR_BRIGHT_WHITE   "\x1b[97m"   /**< Bright white foreground. */

#define ANSI_BACKGROUND_BLACK   "\x1b[40m"   /**< Black background. */
#define ANSI_BACKGROUND_RED     "\x1b[41m"   /**< Red background. */
#define ANSI_BACKGROUND_GREEN   "\x1b[42m"   /**< Green background. */
#define ANSI_BACKGROUND_YELLOW  "\x1b[43m"   /**< Yellow background. */
#define ANSI_BACKGROUND_BLUE    "\x1b[44m"   /**< Blue background. */
#define ANSI_BACKGROUND_MAGENTA "\x1b[45m"   /**< Magenta background. */
#define ANSI_BACKGROUND_CYAN    "\x1b[46m"   /**< Cyan background. */
#define ANSI_BACKGROUND_WHITE   "\x1b[47m"   /**< White background. */

#define ANSI_BACKGROUND_BRIGHT_BLACK   "\x1b[100m"  /**< Bright black background. */
#define ANSI_BACKGROUND_BRIGHT_RED     "\x1b[101m"  /**< Bright red background. */
#define ANSI_BACKGROUND_BRIGHT_GREEN   "\x1b[102m"  /**< Bright green background. */
#define ANSI_BACKGROUND_BRIGHT_YELLOW  "\x1b[103m"  /**< Bright yellow background. */
#define ANSI_BACKGROUND_BRIGHT_BLUE    "\x1b[104m"  /**< Bright blue background. */
#define ANSI_BACKGROUND_BRIGHT_MAGENTA "\x1b[105m"  /**< Bright magenta background. */
#define ANSI_BACKGROUND_BRIGHT_CYAN    "\x1b[106m"  /**< Bright cyan background. */
#define ANSI_BACKGROUND_BRIGHT_WHITE   "\x1b[107m"  /**< Bright white background. */

#define ANSI_STYLE_BOLD         "\x1b[1m"  /**< Bold text. */
#define ANSI_STYLE_ITALIC       "\x1b[3m"  /**< Italic text. */
#define ANSI_STYLE_UNDERLINE    "\x1b[4m"  /**< Underlined text. */

/** @brief Format a string as a red-background error message for the terminal. */
#define TERMINAL_ERROR_MESSAGE(message) "\n" ANSI_BACKGROUND_RED ANSI_COLOR_WHITE ANSI_STYLE_BOLD message ANSI_RESET_ALL "\n"
/** @brief Format a string as a green-background success message for the terminal. */
#define TERMINAL_SUCCESS_MESSAGE(message) "\n" ANSI_BACKGROUND_GREEN ANSI_COLOR_WHITE ANSI_STYLE_BOLD message ANSI_RESET_ALL "\n"
/** @brief Format a string as a magenta-background endpoint message for the terminal. */
#define TERMINAL_ENDPOINT_MESSAGE(message) "\n" ANSI_BACKGROUND_MAGENTA ANSI_COLOR_WHITE message ANSI_RESET_ALL "\n"
/** @brief Format a string as a blue-background SQL message for the terminal. */
#define TERMINAL_SQL_MESSAGE(message) "\n" ANSI_BACKGROUND_BLUE ANSI_COLOR_WHITE message ANSI_RESET_ALL "\n"
