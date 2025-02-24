
#define TOOL_NAME               "2D GI Tracer"
#define TOOL_SHORT_NAME         "2D-GITracer"
#define TOOL_VERSION            "1.0"
#define TOOL_DESCRIPTION        "lorem"
#define TOOL_DESCRIPTION_BREAK  "lorem"
#define TOOL_RELEASE_DATE       "Month.Date"
#define TOOL_LOGO_COLOR         0x7da9b9ff

#include "raylib.h"		// required for for RAYLIB

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_GRID_ALPHA                 0.1f
#define RAYGUI_TEXTSPLIT_MAX_ITEMS        256
#define RAYGUI_TEXTSPLIT_MAX_TEXT_SIZE   4096
#define RAYGUI_TOGGLEGROUP_MAX_ITEMS      256
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"     

#undef RAYGUI_IMPLEMENTATION	// avoid including raygui again

// Layouts
// #define GUI_MAIN_TOOLBAR_IMPLEMENTATION
// #include "gui_main_toolbar.h"               // GUI: Main toolbar

// #define GUI_WINDOW_HELP_IMPLEMENTATION
// #include "gui_window_help.h"                // GUI: Help Window


#include "resource_dir.h" //			    // Resources: Helper

// Standard libraries


//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------

static const char *toolName = TOOL_NAME;
static const char *toolVersion = TOOL_VERSION;
static const char *toolDescription = TOOL_DESCRIPTION;


//----------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------
int main (int argc, char *argv[])
{

	// InitWindow
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
	InitWindow(640, 360, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));
	SetWindowMinSize(480, 220);
    SetExitKey(0);

    SetTargetFPS(60);

	SearchAndSetResourceDir("resources");
	SearchAndSetResourceDir("styles");
	GuiLoadStyle("style_darker.rgs");

	int samples_max = 1024;
	int samples_curr = 0;
	float progress = 0;
	
    // General pourpose variables
    Vector2 mouse = { 0, 0 };               // Mouse position

	while (!WindowShouldClose())
	{
		samples_curr++;
		progress = (float)samples_curr / samples_max;

		// Vector2 anchorRight;
		// Vector2 anchorVisuals;
		
		// anchorRight.x = (float)GetScreenWidth() - 104;       // Update right-anchor panel
		// anchorVisuals.x = anchorRight.x - 324 + 1;    // Update right-anchor panel

		

        BeginDrawing();
			ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
			GuiPanel((Rectangle){0, GetScreenHeight()-30, GetScreenWidth(), 14}, NULL);
			GuiProgressBar((Rectangle){0,  GetScreenHeight()-12, GetScreenWidth(), 12}, "Samples", "gfg" , &progress,  0.0f, 1.0f);
        EndDrawing();


		// BeginDrawing();
		// ClearBackground(BLACK);
		// DrawText("Hello Raylib", 200,200,20,WHITE);
		// DrawTexture(wabbit, 400, 200, WHITE);
		// EndDrawing();
	}

	// UnloadTexture(wabbit);

	CloseWindow();
	return 0;
}
