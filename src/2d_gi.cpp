
// #define TOOL_NAME               "2D Global Illumination"
// #define TOOL_SHORT_NAME         "2D-GI"
// #define TOOL_VERSION            "1.0"
// #define TOOL_DESCRIPTION        ""
// #define TOOL_DESCRIPTION_BREAK  ""
// #define TOOL_RELEASE_DATE       "Month.Date"
// #define TOOL_LOGO_COLOR         0x7da9b9ff

// #include "raylib.h"		// required for for RAYLIB

// #define RAYGUI_IMPLEMENTATION
// #define RAYGUI_GRID_ALPHA                 0.1f
// #define RAYGUI_TEXTSPLIT_MAX_ITEMS        256
// #define RAYGUI_TEXTSPLIT_MAX_TEXT_SIZE   4096
// #define RAYGUI_TOGGLEGROUP_MAX_ITEMS      256
// #define RAYGUI_IMPLEMENTATION
// #include "raygui.h"     

// #undef RAYGUI_IMPLEMENTATION	// avoid including raygui again

// // Layouts
// // #define GUI_MAIN_TOOLBAR_IMPLEMENTATION
// // #include "gui_main_toolbar.h"               // GUI: Main toolbar

// // #define GUI_WINDOW_HELP_IMPLEMENTATION
// // #include "gui_window_help.h"                // GUI: Help Window


// #include "resource_dir.h" //			    // Resources: Helper

// // Standard libraries


// //----------------------------------------------------------------------------------
// // Global Variables Definition
// //----------------------------------------------------------------------------------

// static const char *toolName = TOOL_NAME;
// static const char *toolVersion = TOOL_VERSION;
// static const char *toolDescription = TOOL_DESCRIPTION;


// //----------------------------------------------------------------------------------
// // Program main entry point
// //----------------------------------------------------------------------------------
// int main (int argc, char *argv[])
// {

// 	// InitWindow
// 	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
// 	InitWindow(640, 360, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));
// 	SetWindowMinSize(480, 220);
//     SetExitKey(0);

//     SetTargetFPS(60);

// 	SearchAndSetResourceDir("resources");
// 	SearchAndSetResourceDir("styles");
// 	GuiLoadStyle("style_darker.rgs");

// 	int samples_max = 1024;
// 	int samples_curr = 0;
// 	float progress = 0;
	
//     // General pourpose variables
//     Vector2 mouse = { 0, 0 };               // Mouse position

// 	while (!WindowShouldClose())
// 	{
// 		samples_curr++;
// 		progress = (float)samples_curr / samples_max;

// 		// Vector2 anchorRight;
// 		// Vector2 anchorVisuals;
		
// 		// anchorRight.x = (float)GetScreenWidth() - 104;       // Update right-anchor panel
// 		// anchorVisuals.x = anchorRight.x - 324 + 1;    // Update right-anchor panel
		
//         BeginDrawing();
// 			ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
// 			GuiPanel((Rectangle){0, GetScreenHeight()-30, GetScreenWidth(), 30}, NULL);
// 			GuiDrawText("Hello", (Rectangle){0,  GetScreenHeight()-50, GetScreenWidth(), 12}, 0, WHITE);
// 			GuiProgressBar((Rectangle){0,  GetScreenHeight()-12, GetScreenWidth(), 12}, "", "" , &progress,  0.0f, 1.0f);
//         EndDrawing();

// 		// BeginDrawing();
// 		// ClearBackground(BLACK);
// 		// DrawText("Hello Raylib", 200,200,20,WHITE);
// 		// DrawTexture(wabbit, 400, 200, WHITE);
// 		// EndDrawing();
// 	}

// 	// UnloadTexture(wabbit);

// 	CloseWindow();
// 	return 0;
// }
// /*******************************************************************************************
// *
// *   raylib [rlgl] example - compute shader - Conway's Game of Life
// *
// *   NOTE: This example requires raylib OpenGL 4.3 versions for compute shaders support,
// *         shaders used in this example are #version 430 (OpenGL 4.3)
// *
// *   Example complexity rating: [★★★★] 4/4
// *
// *   Example originally created with raylib 4.0, last time updated with raylib 2.5
// *
// *   Example contributed by Teddy Astie (@tsnake41) and reviewed by Ramon Santamaria (@raysan5)
// *
// *   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
// *   BSD-like license that allows static linking with closed source software
// *
// *   Copyright (c) 2021-2025 Teddy Astie (@tsnake41)
// *
// ********************************************************************************************/

#include <stdlib.h>


#include "raylib.h"
#include "rlgl.h"
#include "resource_dir.h"


#define RLGL_IMPLEMENTATION
#define RLGL_RENDER_TEXTURES_HINT
#define RLGL_SHOW_GL_DETAILS_INFO
#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT

// IMPORTANT: This must match gol*.glsl GOL_WIDTH constant.
// This must be a multiple of 16 (check golLogic compute dispatch).
#define GOL_WIDTH 768

// Maximum amount of queued draw commands (squares draw from mouse down events).
#define MAX_BUFFERED_TRANSFERTS 48

// Game Of Life Update Command
typedef struct GolUpdateCmd {
    unsigned int x;         // x coordinate of the gol command
    unsigned int y;         // y coordinate of the gol command
    unsigned int w;         // width of the filled zone
    unsigned int enabled;   // whether to enable or disable zone
} GolUpdateCmd;

// Game Of Life Update Commands SSBO
typedef struct GolUpdateSSBO {
    unsigned int count;
    GolUpdateCmd commands[MAX_BUFFERED_TRANSFERTS];
} GolUpdateSSBO;

#define GRAPHICS_API_OPENGL_43

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{


	SearchAndSetResourceDir("resources");
	SearchAndSetResourceDir("shaders");

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(GOL_WIDTH, GOL_WIDTH, "raylib [rlgl] example - compute shader - game of life");

    const Vector2 resolution = { GOL_WIDTH, GOL_WIDTH };
    unsigned int brushSize = 8;


    // Game of Life logic compute shader
    char *golLogicCode = LoadFileText("gol.glsl");
    unsigned int golLogicShader = rlCompileShader(golLogicCode, RL_COMPUTE_SHADER);
    unsigned int golLogicProgram = rlLoadComputeShaderProgram(golLogicShader);
    UnloadFileText(golLogicCode);

    // Game of Life logic render shader
    Shader golRenderShader = LoadShader(NULL, "gol_render.glsl");
    int resUniformLoc = GetShaderLocation(golRenderShader, "resolution");

    // Game of Life transfert shader (CPU<->GPU download and upload)
    char *golTransfertCode = LoadFileText("gol_transfert.glsl");
    unsigned int golTransfertShader = rlCompileShader(golTransfertCode, RL_COMPUTE_SHADER);
    unsigned int golTransfertProgram = rlLoadComputeShaderProgram(golTransfertShader);
    UnloadFileText(golTransfertCode);

    // Load shader storage buffer object (SSBO), id returned
    unsigned int ssboA = rlLoadShaderBuffer(GOL_WIDTH*GOL_WIDTH*sizeof(unsigned int), NULL, RL_DYNAMIC_COPY);
    unsigned int ssboB = rlLoadShaderBuffer(GOL_WIDTH*GOL_WIDTH*sizeof(unsigned int), NULL, RL_DYNAMIC_COPY);
    unsigned int ssboTransfert = rlLoadShaderBuffer(sizeof(GolUpdateSSBO), NULL, RL_DYNAMIC_COPY);

    GolUpdateSSBO transfertBuffer = { 0 };

    // Create a white texture of the size of the window to update
    // each pixel of the window using the fragment shader: golRenderShader
    Image whiteImage = GenImageColor(GOL_WIDTH, GOL_WIDTH, WHITE);
    Texture whiteTex = LoadTextureFromImage(whiteImage);
    UnloadImage(whiteImage);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        brushSize += (int)GetMouseWheelMove();

        if ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            && (transfertBuffer.count < MAX_BUFFERED_TRANSFERTS))
        {
            // Buffer a new command
            transfertBuffer.commands[transfertBuffer.count].x = GetMouseX() - brushSize/2;
            transfertBuffer.commands[transfertBuffer.count].y = GetMouseY() - brushSize/2;
            transfertBuffer.commands[transfertBuffer.count].w = brushSize;
            transfertBuffer.commands[transfertBuffer.count].enabled = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            transfertBuffer.count++;
        }
        else if (transfertBuffer.count > 0)  // Process transfert buffer
        {
            // Send SSBO buffer to GPU
            rlUpdateShaderBuffer(ssboTransfert, &transfertBuffer, sizeof(GolUpdateSSBO), 0);

            // Process SSBO commands on GPU
            rlEnableShader(golTransfertProgram);
            rlBindShaderBuffer(ssboA, 1);
            rlBindShaderBuffer(ssboTransfert, 3);
            rlComputeShaderDispatch(transfertBuffer.count, 1, 1); // Each GPU unit will process a command!
            rlDisableShader();

            transfertBuffer.count = 0;
        }
        else
        {
            // Process game of life logic
            rlEnableShader(golLogicProgram);
            rlBindShaderBuffer(ssboA, 1);
            rlBindShaderBuffer(ssboB, 2);
            rlComputeShaderDispatch(GOL_WIDTH/16, GOL_WIDTH/16, 1);
            rlDisableShader();

            // ssboA <-> ssboB
            int temp = ssboA;
            ssboA = ssboB;
            ssboB = temp;
        }

        rlBindShaderBuffer(ssboA, 1);
        SetShaderValue(golRenderShader, resUniformLoc, &resolution, SHADER_UNIFORM_VEC2);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLANK);

            BeginShaderMode(golRenderShader);
                DrawTexture(whiteTex, 0, 0, WHITE);
            EndShaderMode();

            DrawRectangleLines(GetMouseX() - brushSize/2, GetMouseY() - brushSize/2, brushSize, brushSize, RED);

            DrawText("Use Mouse wheel to increase/decrease brush size", 10, 10, 20, WHITE);
            DrawFPS(GetScreenWidth() - 100, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload shader buffers objects.
    rlUnloadShaderBuffer(ssboA);
    rlUnloadShaderBuffer(ssboB);
    rlUnloadShaderBuffer(ssboTransfert);

    // Unload compute shader programs
    rlUnloadShaderProgram(golTransfertProgram);
    rlUnloadShaderProgram(golLogicProgram);

    UnloadTexture(whiteTex);            // Unload white texture
    UnloadShader(golRenderShader);      // Unload rendering fragment shader

    CloseWindow();                      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
