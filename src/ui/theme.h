#pragma once

// ----------------------------------------------------------------------------
// HELPER MACROS
// -----------------------------------------------------------------------------

// Color Conversion
#define RGBA255(r, g, b, a) ImVec4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a) / 255.0f)

// Text Scaling
#define REM(x)     ((F32)(mscbl_config.settings.font_size * (x)))
#define RADIUS(x)  (REM(0.625f) * (F32)(x))
#define SPACING(x) (REM(0.25f) * (F32)(x))

// ----------------------------------------------------------------------------
// VALUE MACROS
// -----------------------------------------------------------------------------

// Common colors
#define COLOR_TRANSPARENT RGBA255(0, 0, 0, 0)

// Structural Backgrounds
#define MSCBL_BACKGROUND       RGBA255(13, 13, 16, 255)
#define MSCBL_BACKGROUND_HOVER RGBA255(18, 17, 22, 255)
#define MSCBL_SURFACE          RGBA255(22, 21, 28, 255)
#define MSCBL_POPOVER          RGBA255(10, 10, 12, 255)

// Component Accents & Interactions
#define MSCBL_PRIMARY            RGBA255(245, 2, 67, 255)
#define MSCBL_PRIMARY_HOVER      RGBA255(245, 2, 67, 40)
#define MSCBL_INTERACTION        RGBA255(28, 27, 34, 255)
#define MSCBL_INTERACTION_HOVER  RGBA255(38, 36, 46, 255)
#define MSCBL_INTERACTION_ACTIVE RGBA255(46, 44, 56, 255)

// Widget Filter Accent States
#define MSCBL_STATE_ADDITIVE    RGBA255(34, 197, 94, 255)
#define MSCBL_STATE_SUBTRACTIVE RGBA255(239, 68, 68, 255)

// Borders & Division
#define MSCBL_BORDER       RGBA255(46, 44, 54, 255)
#define MSCBL_BORDER_MUTED RGBA255(26, 25, 31, 255)

// Typography Weights
#define MSCBL_FOREGROUND       RGBA255(242, 240, 245, 255)
#define MSCBL_FOREGROUND_MUTED RGBA255(148, 144, 158, 255)

// Sizing and spacing
#define MSCBL_OUTER_PADDING SPACING(2.0f)
#define MSCBL_INNER_PADDING SPACING(1.0f)
#define MSCBL_INDENT        SPACING(2.5f)
