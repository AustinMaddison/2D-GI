#define TOOL_NAME               "2D Global Illumination"
#define TOOL_SHORT_NAME         "2D-GI"
#define TOOL_VERSION            "1.0"
#define TOOL_DESCRIPTION        "GPU Benchmark Render Engine"
#define TOOL_DESCRIPTION_BREAK  ""
#define TOOL_RELEASE_DATE       "Month.Date"
#define TOOL_LOGO_COLOR         0x7da9b9ff

#include <stdlib.h>
#include <iostream>
#include <map>
#include <algorithm>

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

#define RLGL_IMPLEMENTATION
#define RLGL_RENDER_TEXTURES_HINT
#define RLGL_SHOW_GL_DETAILS_INFO
#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT
#define GRAPHICS_API_OPENGL_43

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "resource_dir.h"

static const char *toolName = TOOL_NAME;
static const char *toolVersion = TOOL_VERSION;
static const char *toolDescription = TOOL_DESCRIPTION;

#define MAP_WIDTH 768
#define SAMPLES_MAX 1000

typedef std::map<char, int> UniformMap;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Resources
    /* -------------------------------------------------------------------------- */
	SearchAndSetResourceDir("resources");

    // Initialization
    //--------------------------------------------------------------------------------------
	InitWindow(MAP_WIDTH, MAP_WIDTH, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));
    const Vector2 resolution = { MAP_WIDTH, MAP_WIDTH };
	GuiLoadStyle("styles/style_darker.rgs");
    HideCursor();
    // SetTargetFPS(60);



    
    /* -------------------------------------------------------------------------- */
    /*                               Compute Shaders                              */
    /* -------------------------------------------------------------------------- */
    char *sceneShaderCode = LoadFileText("shaders/scene_map.glsl");
    unsigned int sceneShader = rlCompileShader(sceneShaderCode, RL_COMPUTE_SHADER);
    unsigned int sceneProgram = rlLoadComputeShaderProgram(sceneShader);
    UnloadFileText(sceneShaderCode);

    char *raytraceGiShaderCode = LoadFileText("shaders/raytrace_gi.glsl");
    unsigned int raytraceGiShader = rlCompileShader(raytraceGiShaderCode, RL_COMPUTE_SHADER);
    unsigned int raytraceGiProgram = rlLoadComputeShaderProgram(raytraceGiShader);
    UnloadFileText(raytraceGiShaderCode);

    // buffer for compute shaders
    unsigned int ssboSceneMap = rlLoadShaderBuffer(MAP_WIDTH*MAP_WIDTH*sizeof(float), NULL, RL_DYNAMIC_COPY);
    unsigned int ssboGiMapA = rlLoadShaderBuffer(MAP_WIDTH*MAP_WIDTH*sizeof(float)*4, NULL, RL_DYNAMIC_COPY);
    unsigned int ssboGiMapB = rlLoadShaderBuffer(MAP_WIDTH*MAP_WIDTH*sizeof(float)*4, NULL, RL_DYNAMIC_COPY);
    unsigned int ssboGenRayCount = rlLoadShaderBuffer(MAP_WIDTH*MAP_WIDTH*sizeof(uint), NULL, RL_DYNAMIC_COPY);

    /* -------------------------------------------------------------------------- */
    /*                           Output Fragment Shader                           */
    /* -------------------------------------------------------------------------- */
    Shader sceneRenderShader = LoadShader(NULL, "shaders/scene_map_render.glsl");

    int resUniformLoc = GetShaderLocation(sceneRenderShader, "resolution");

    int samplesUniformLoc = rlGetLocationUniform(raytraceGiProgram, "samples");
    
    // Just to apply fragment shader for ouput
    Image whiteImage = GenImageColor(MAP_WIDTH, MAP_WIDTH, WHITE);
    Texture outputTex = LoadTextureFromImage(whiteImage);
    UnloadImage(whiteImage);

    

    //--------------------------------------------------------------------------------------
    
    float distClosestSurface = 0;
    uint samples = 0;
    float renderProgress=0;
    double time = GetTime();

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        // WaitTime(1);
        if(samples < SAMPLES_MAX && GetTime()-time > 0.1)
        {
            time = GetTime();
            // Generate SDF Map
            rlEnableShader(sceneProgram);
            rlBindShaderBuffer(ssboSceneMap, 1);
            rlComputeShaderDispatch(MAP_WIDTH/16, MAP_WIDTH/16, 1);
            rlDisableShader();

            // Raytrace
            rlEnableShader(raytraceGiProgram);
            rlSetUniform(samplesUniformLoc, &samples, SHADER_UNIFORM_UINT, 1);
            rlBindShaderBuffer(ssboSceneMap, 1);
            rlBindShaderBuffer(ssboGenRayCount, 2);
            rlBindShaderBuffer(ssboGiMapA, 3);
            rlBindShaderBuffer(ssboGiMapB, 4);
            rlComputeShaderDispatch(MAP_WIDTH/16, MAP_WIDTH/16, 1);
            rlDisableShader();
            samples++;
            renderProgress = (float)samples/SAMPLES_MAX;

            std::swap(ssboGiMapA, ssboGiMapB);
        }

        SetShaderValue(sceneRenderShader, resUniformLoc, &resolution, SHADER_UNIFORM_VEC2);

        // Draw
        BeginDrawing();
            ClearBackground(BLANK);

            BeginShaderMode(sceneRenderShader);
                rlBindShaderBuffer(ssboSceneMap, 1);
                rlBindShaderBuffer(ssboGiMapA, 3);
                DrawTexture(outputTex, 0, 0, WHITE);
            EndShaderMode();


            if(IsCursorOnScreen())
            {
                rlReadShaderBuffer(ssboSceneMap, &distClosestSurface, sizeof(float), (GetMouseX() + GetMouseY() * MAP_WIDTH) * sizeof(float));
                u_char distNorm = distClosestSurface/((float)MAP_WIDTH*0.5) * 255;
                DrawCircleLines(GetMouseX(), GetMouseY(), distClosestSurface, RED);
                DrawCircle(GetMouseX(), GetMouseY(), 2, {distNorm, 0, 0, 255});
            }

            DrawRectangle(0, 0, 314, 90, {0, 0, 0, 200});
            DrawText(TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription), 10, 10, 10, Color({255, 255, 255, 100}));
            DrawText(TextFormat("%s %d %d", "MouseXY", GetMouseX(), GetMouseY()), 10, 30, 10, Color({255, 255, 255, 255}));
            DrawText(TextFormat("%s %f", "Distance To Closest Surface", distClosestSurface), 10, 50, 10, Color({255, 255, 255, 255}));
            DrawText(TextFormat("%s %d", "FPS", GetFPS()), 10, 70, 10, Color({255, 255, 255, 255}));
            DrawRectangle(0, GetScreenHeight()-12, GetScreenWidth(), 12, {0, 0, 0, 200});
            GuiProgressBar((Rectangle){0,  GetScreenHeight()-12, GetScreenWidth(), 12}, "", "" , &renderProgress,  0.0f, 1.0f);


            
            // uint genenRayCount = 0;
            // rlReadShaderBuffer(ssboSceneMap, &genenRayCount, sizeof(uint), (GetMouseX() + GetMouseY() * MAP_WIDTH) * sizeof(uint));
            // std::cout << genenRayCount << '\n';

            
        EndDrawing();

        // SetTargetFPS(60);
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload shader buffers objects.
    rlUnloadShaderBuffer(ssboSceneMap);
    rlUnloadShaderBuffer(ssboGiMapA);
    rlUnloadShaderBuffer(ssboGiMapB);

    // Unload compute shader programs
    rlUnloadShaderProgram(raytraceGiProgram);
    rlUnloadShaderProgram(sceneProgram);

    UnloadTexture(outputTex);               
    UnloadShader(sceneRenderShader);      // Unload rendering fragment shader

    CloseWindow();                      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
