#include "config.h"

#define RGBA255(r, g, b, a) ImVec4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a) / 255.0f)

// https://ui.shadcn.com/themes - Rose theme

// TODO: Light mode colors are wrong

// :root {
//   --background: oklch(1 0 0);
#define LIGHT_BACKGROUND ImVec4(1, 1, 1, 1)
//   --foreground: oklch(0.141 0.005 285.823);
#define LIGHT_FOREGROUND ImVec4(0.00274, 0.00274, 0.00344, 1)
//   --card: oklch(1 0 0);
#define LIGHT_CARD ImVec4(1, 1, 1, 1)
//   --card-foreground: oklch(0.141 0.005 285.823);
#define LIGHT_CARD_FOREGROUND ImVec4(0.00274, 0.00274, 0.00344, 1)
//   --popover: oklch(1 0 0);
#define LIGHT_POPOVER ImVec4(1, 1, 1, 1)
//   --popover-foreground: oklch(0.141 0.005 285.823);
#define LIGHT_POPOVER_FOREGROUND ImVec4(0.00274, 0.00274, 0.00344, 1)
//   --primary: oklch(0.586 0.253 17.585);
#define LIGHT_PRIMARY ImVec4(0.84271, -0.01824, 0.05036, 1)
//   --primary-foreground: oklch(0.969 0.015 12.422);
#define LIGHT_PRIMARY_FOREGROUND ImVec4(0.99778, 0.87884, 0.88704, 1)
//   --secondary: oklch(0.967 0.001 286.375);
#define LIGHT_SECONDARY ImVec4(0.90364, 0.90364, 0.90999, 1)
//   --secondary-foreground: oklch(0.21 0.006 285.885);
#define LIGHT_SECONDARY_FOREGROUND ImVec4(0.00909, 0.00909, 0.01095, 1)
//   --muted: oklch(0.967 0.001 286.375);
#define LIGHT_MUTED ImVec4(0.90364, 0.90364, 0.90999, 1)
//   --muted-foreground: oklch(0.552 0.016 285.938);
#define LIGHT_MUTED_FOREGROUND ImVec4(0.16502, 0.16497, 0.19936, 1)
//   --accent: oklch(0.967 0.001 286.375);
#define LIGHT_ACCENT ImVec4(0.90364, 0.90364, 0.90999, 1)
//   --accent-foreground: oklch(0.21 0.006 285.885);
#define LIGHT_ACCENT_FOREGROUND ImVec4(0.00909, 0.00909, 0.01095, 1)
//   --destructive: oklch(0.577 0.245 27.325);
//   --border: oklch(0.92 0.004 286.32);
#define LIGHT_BORDER ImVec4(0.77653, 0.77653, 0.79964, 1)
//   --input: oklch(0.92 0.004 286.32);
#define LIGHT_INPUT ImVec4(0.77653, 0.77653, 0.79964, 1)
//   --ring: oklch(0.712 0.194 13.428);
//   --chart-1: oklch(0.81 0.117 11.638);
//   --chart-2: oklch(0.645 0.246 16.439);
//   --chart-3: oklch(0.586 0.253 17.585);
//   --chart-4: oklch(0.514 0.222 16.935);
//   --chart-5: oklch(0.455 0.188 13.697);
//   --sidebar: oklch(0.985 0 0);
#define LIGHT_SIDEBAR ImVec4(0.95567, 0.95567, 0.95567, 1)
//   --sidebar-foreground: oklch(0.141 0.005 285.823);
#define LIGHT_SIDEBAR_FOREGROUND ImVec4(0.00274, 0.00274, 0.00344, 1)
//   --sidebar-primary: oklch(0.586 0.253 17.585);
#define LIGHT_SIDEBAR_PRIMARY ImVec4(0.84271, -0.01824, 0.05036, 1)
//   --sidebar-primary-foreground: oklch(0.969 0.015 12.422);
#define LIGHT_SIDEBAR_PRIMARY_FOREGROUND ImVec4(0.99778, 0.87884, 0.88704, 1)
//   --sidebar-accent: oklch(0.967 0.001 286.375);
#define LIGHT_SIDEBAR_ACCENT ImVec4(0.90364, 0.90364, 0.90999, 1)
//   --sidebar-accent-foreground: oklch(0.21 0.006 285.885);
#define LIGHT_SIDEBAR_ACCENT_FOREGROUND ImVec4(0.00909, 0.00909, 0.01095, 1)
//   --sidebar-border: oklch(0.92 0.004 286.32);
#define LIGHT_SIDEBAR_BORDER ImVec4(0.77653, 0.77653, 0.79964, 1)
//   --sidebar-ring: oklch(0.712 0.194 13.428);
// }

// .dark {
//   --background: oklch(0.141 0.005 285.823);
#define DARK_BACKGROUND       ImVec4(RGBA255(9, 9, 11, 255))
#define DARK_BACKGROUND_HOVER ImVec4(RGBA255(9, 9, 11, 204))
//   --foreground: oklch(0.985 0 0);
#define DARK_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --card: oklch(0.21 0.006 285.885);
#define DARK_CARD ImVec4(RGBA255(24, 24, 27, 255))
//   --card-foreground: oklch(0.985 0 0);
#define DARK_CARD_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --popover: oklch(0.21 0.006 285.885);
#define DARK_POPOVER ImVec4(RGBA255(24, 24, 27, 255))
//   --popover-foreground: oklch(0.985 0 0);
#define DARK_POPOVER_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --primary: oklch(0.645 0.246 16.439);
#define DARK_PRIMARY       ImVec4(RGBA255(255, 32, 86, 255))
#define DARK_PRIMARY_HOVER ImVec4(RGBA255(255, 32, 86, 127))
//   --primary-foreground: oklch(0.969 0.015 12.422);
#define DARK_PRIMARY_FOREGROUND ImVec4(RGBA255(255, 241, 242, 255))
//   --secondary: oklch(0.274 0.006 286.033);
#define DARK_SECONDARY        ImVec4(RGBA255(39, 39, 42, 255))
#define DARK_SECONDARY_HOVER  ImVec4(RGBA255(39, 39, 42, 204))
#define DARK_SECONDARY_ACTIVE ImVec4(RGBA255(42, 42, 45, 230))
//   --secondary-foreground: oklch(0.985 0 0);
#define DARK_SECONDARY_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --muted: oklch(0.274 0.006 286.033);
#define DARK_MUTED ImVec4(RGBA255(39, 39, 42, 255))
//   --muted-foreground: oklch(0.705 0.015 286.067);
#define DARK_MUTED_FOREGROUND ImVec4(RGBA255(159, 159, 169, 255))
//   --accent: oklch(0.274 0.006 286.033);
#define DARK_ACCENT       ImVec4(RGBA255(39, 39, 42, 255))
#define DARK_ACCENT_HOVER ImVec4(RGBA255(39, 39, 42, 127))
//   --accent-foreground: oklch(0.985 0 0);
#define DARK_ACCENT_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --destructive: oklch(0.704 0.191 22.216);
//   --border: oklch(1 0 0 / 10%);
#define DARK_BORDER ImVec4(RGBA255(255, 255, 255, 25.5))
//   --input: oklch(1 0 0 / 15%);
#define DARK_INPUT ImVec4(RGBA255(255, 255, 255, 15))
//   --ring: oklch(0.41 0.159 10.272);
//   --chart-1: oklch(0.81 0.117 11.638);
//   --chart-2: oklch(0.645 0.246 16.439);
//   --chart-3: oklch(0.586 0.253 17.585);
//   --chart-4: oklch(0.514 0.222 16.935);
//   --chart-5: oklch(0.455 0.188 13.697);
//   --sidebar: oklch(0.21 0.006 285.885);
#define DARK_SIDEBAR ImVec4(RGBA255(24, 24, 27, 255))
//   --sidebar-foreground: oklch(0.985 0 0);
#define DARK_SIDEBAR_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --sidebar-primary: oklch(0.645 0.246 16.439);
#define DARK_SIDEBAR_PRIMARY ImVec4(RGBA255(255, 32, 86, 255))
//   --sidebar-primary-foreground: oklch(0.969 0.015 12.422);
#define DARK_SIDEBAR_PRIMARY_FOREGROUND ImVec4(RGBA255(255, 241, 242, 255))
//   --sidebar-accent: oklch(0.274 0.006 286.033);
#define DARK_SIDEBAR_ACCENT ImVec4(RGBA255(39, 39, 42, 255))
//   --sidebar-accent-foreground: oklch(0.985 0 0);
#define DARK_SIDEBAR_ACCENT_FOREGROUND ImVec4(RGBA255(250, 250, 250, 255))
//   --sidebar-border: oklch(1 0 0 / 10%);
#define DARK_SIDEBAR_BORDER ImVec4(RGBA255(255, 255, 255, 25.5))
//   --sidebar-ring: oklch(0.41 0.159 10.272);
// }

// Stupid OKLCH conversion took 3 hours (Only if they posted the OKLCH to RGBA conversion formula online)

#define REM(x)     ((float)(mscbl_config.settings.font_size * (x)))
#define RADIUS(x)  REM(0.625) * (float)(x)
#define SPACING(x) REM(0.25) * (float)(x)
