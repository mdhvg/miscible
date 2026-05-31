#pragma once

// ----------------------------------------------------------------------------
// HELPER MACROS
// -----------------------------------------------------------------------------

// Color Conversion
#define RGBA255(r, g, b, a) ImVec4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a) / 255.0f)

// Text Scaling
#define REM(x)     ((F32)(mscbl_config.settings.font_size * (x)))
#define RADIUS(x)  (REM(0.625f) * (float)(x))
#define SPACING(x) (REM(0.25f) * (float)(x))

// ----------------------------------------------------------------------------
// VALUE MACROS
// -----------------------------------------------------------------------------

// Common colors
#define COLOR_TRANSPARENT RGBA255(0, 0, 0, 0)

// Structural Backgrounds
#define MSCBL_BACKGROUND       RGBA255(11, 11, 13, 255)
#define MSCBL_BACKGROUND_HOVER RGBA255(18, 17, 21, 255)
#define MSCBL_SURFACE          RGBA255(15, 14, 17, 255)
#define MSCBL_POPOVER          RGBA255(9, 8, 10, 255)

// Component Accents & Interactions
#define MSCBL_PRIMARY            RGBA255(245, 2, 67, 255)
#define MSCBL_PRIMARY_HOVER      RGBA255(245, 2, 67, 140)
#define MSCBL_INTERACTION        RGBA255(24, 23, 28, 255)
#define MSCBL_INTERACTION_HOVER  RGBA255(34, 32, 40, 255)
#define MSCBL_INTERACTION_ACTIVE RGBA255(44, 41, 52, 255)

// Widget Filter Accent States
#define MSCBL_STATE_ADDITIVE    RGBA255(0, 230, 118, 255)
#define MSCBL_STATE_SUBTRACTIVE RGBA255(195, 30, 30, 255)

// Borders & Division
#define MSCBL_BORDER       RGBA255(32, 30, 37, 255)
#define MSCBL_BORDER_MUTED RGBA255(22, 21, 26, 255)

// Typography Weights
#define MSCBL_FOREGROUND       RGBA255(230, 228, 232, 255)
#define MSCBL_FOREGROUND_MUTED RGBA255(126, 122, 133, 255)

// Sizing and spacing
#define MSCBL_OUTER_PADDING SPACING(2.0f)
#define MSCBL_INNER_PADDING SPACING(1.0f)
#define MSCBL_INDENT        SPACING(2.5f)
