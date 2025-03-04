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
#include <vector>

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

#define DEFAULT_WIDTH 720
#define DEFUALT_HEIGHT 720

#define SAMPLES_MAX 1024
#define SNAPSHOT 1023

std::map<uint, uint> resolutionUniformLocations;

enum RenderType {
    RAYTRACE,
    RADIENCE_PROBES,
    RADIENCE_CASCADES
};

enum RenderStatus {
    RUNNING,
    PAUSED,
    FINISHED
};

typedef struct {
    RenderType type;
    RenderStatus status;
    uint samplesCurr;
    uint samplesMax;
    float timeInitial;
    float timeElapsed;

    int resolution_width;
    int resolution_height;
    Vector2 mousePos;

    float frameTime; // ms
    float fps; // frames per second
    float rps; // rays per second

    float distClosestSurface;

    uint sdfSceneProgram;
    uint giRayTraceProgram;
    uint giRadienceProbesProgram;
    uint giRadienceCascadesProgram;
    uint normalSceneProgram;
    uint compositeProgram;
    uint postProcessProgram;

    Shader finalPassShader;

    uint ssboSceneColor;
    uint ssboSceneMask;
    uint ssboSceneSdf;
    uint ssboGiMapA;
    uint ssboGiMapB;
    uint ssboPostProcess;

    std::map<uint, uint> sampleCurrUniformLocs;
    std::map<uint, uint> resolutionUniformLocs;

    Texture textureOut
} AppState;

void CreateAppState(AppState *state)
{
    SearchAndSetResourceDir("resources");
    GuiLoadStyle("styles/style_darker.rgs");
    HideCursor();

    state->type = RAYTRACE;
    state->status = RUNNING;
    state->samplesCurr = 0;
    state->samplesMax = SAMPLES_MAX;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;

    state->resolution_width = GetScreenWidth();
    state->resolution_height = GetScreenHeight();

    state->mousePos = Vector2({0, 0});

    state->frameTime = 0;
    state->fps = 0;
    state->distClosestSurface = 0;

    CreateRenderPipeline(state);
}

void RestartRenderer(AppState *state)
{
    state->samplesCurr = 0;
    state->samplesMax = SAMPLES_MAX;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;

    DeleteRenderPipeline(state);
    CreateRenderPipeline(state);
}

void UpdateState(AppState *state)
{
    if (IsWindowResized())
    {
        RestartRenderer(state);
    }

     state->samplesCurr++;
     state->timeElapsed = GetTime() - state->timeInitial;

     state->mousePos = GetMousePosition();

     state->frameTime = GetFrameTime();

     rlReadShaderBuffer(state->ssboSceneSdf, &state->distClosestSurface, sizeof(float), (GetMouseX() + GetMouseY()) * sizeof(float));
}

void CreateRenderPipeline(AppState *state)
{
    /* --------------------------------- SHADER --------------------------------- */
    char *shaderCode;
    uint compiledShader;
    
    shaderCode = LoadFileText("shaders/sdf_scene.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->sdfSceneProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    shaderCode = LoadFileText("shaders/normal_scene.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->normalSceneProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    // GI 
    shaderCode = LoadFileText("shaders/gi_raytrace.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->giRayTraceProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    shaderCode = LoadFileText("shaders/gi_raytrace.glsl"); // TODO: change
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->giRadienceProbesProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    shaderCode = LoadFileText("shaders/gi_raytrace.glsl"); // TODO: change
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->giRadienceCascadesProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    shaderCode = LoadFileText("shaders/post_process.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->postProcessProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    state->finalPassShader = LoadShader(NULL, "shaders/final_pass.frag");


    for(auto program : {state->giRayTraceProgram, state->giRayTraceProgram, state->giRayTraceProgram})
    {
        state->sampleCurrUniformLocs[program] = rlGetLocationUniform(state->giRayTraceProgram, "samplesCurr");
        state->resolutionUniformLoc = GetShaderLocation(sceneRenderShader, "resolution");
    }



    

    /* ---------------------------------- SSBO ---------------------------------- */
    uint size = state->resolution_width * state->resolution_height;
    state->ssboSceneColor = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY);
    state->ssboSceneMask = rlLoadShaderBuffer(size*sizeof(float), NULL, RL_DYNAMIC_COPY);
    state->ssboSceneSdf = rlLoadShaderBuffer(size*sizeof(float), NULL, RL_DYNAMIC_COPY);
    state->ssboGiMapA = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY); 
    state->ssboGiMapB = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY); 
    state->ssboPostProcess = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY);

}

void DeleteRenderPipeline(AppState *state)
{
    
}

AppState state;

int main(void)
{

	InitWindow(state.resolution_width, state.resolution_height, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));
    CreateAppState(&state);

    /* -------------------------------------------------------------------------- */
    /*                           Output Fragment Shader                           */
    /* -------------------------------------------------------------------------- */


    // Just to apply fragment shader for ouput
    Image whiteImage = GenImageColor(MAP_WIDTH, MAP_WIDTH, MAGENTA);
    Texture outputTex = LoadTextureFromImage(whiteImage);
    UnloadImage(whiteImage);


    //--------------------------------------------------------------------------------------
    
    float distClosestSurface = 0;
    uint samples = 0;
    float renderProgress=0;
    double time = GetTime();


    // Generate SDF Map
    rlEnableShader(sceneProgram);
    rlBindShaderBuffer(ssboSceneMap, 1);
    rlComputeShaderDispatch(MAP_WIDTH/16, MAP_WIDTH/16, 1);
    rlDisableShader();

    // Main game loop
    while (!WindowShouldClose())
    {


        // Update
        // WaitTime(1);
        if(samples < SAMPLES_MAX)
        {
            time = GetTime();

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



            #ifdef SNAPSHOT
            if(samples == SNAPSHOT)
            {
                RenderTexture2D renderTarget = LoadRenderTexture(resolution.x, resolution.y);
                
                BeginTextureMode(renderTarget);
                    BeginShaderMode(sceneRenderShader);
                        rlBindShaderBuffer(ssboSceneMap, 1);
                        rlBindShaderBuffer(ssboGiMapB, 3);
                        DrawTexture(outputTex, 0, 0, WHITE);
                    EndShaderMode();
                EndTextureMode();

                Image img = LoadImageFromTexture(renderTarget.texture); 
                ExportImage(img, TextFormat("%d_samples.png", SNAPSHOT));  
                UnloadImage(img);  
            
                UnloadRenderTexture(renderTarget);
            }
            #endif // SNAPSHOT

            
            if(IsCursorOnScreen())
            {
                rlReadShaderBuffer(ssboSceneMap, &distClosestSurface, sizeof(float), (GetMouseX() + GetMouseY() * MAP_WIDTH) * sizeof(float));
                u_char distNorm = distClosestSurface/((float)MAP_WIDTH*0.5) * 255;
                DrawCircleLines(GetMouseX(), GetMouseY(), distClosestSurface, RED);
                DrawCircle(GetMouseX(), GetMouseY(), 2, {distNorm, 0, 0, 255});
            }

            Vector2 anchor = Vector2({0,0});
            DrawRectangle(0, 0, 314, 90, {0, 0, 0, 200});
            DrawText(TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription), 10, 10, 10, Color({255, 255, 255, 100}));
            DrawText(TextFormat("%s %d %d", "MouseXY", GetMouseX(), GetMouseY()), 10, 30, 10, Color({255, 255, 255, 255}));
            DrawText(TextFormat("%s %f", "Distance To Closest Surface", distClosestSurface), 10, 50, 10, Color({255, 255, 255, 255}));
            DrawText(TextFormat("%s %d", "FPS", GetFPS()), 10, 70, 10, Color({255, 255, 255, 255}));
            DrawRectangle(0, GetScreenHeight()-12, GetScreenWidth(), 12, {0, 0, 0, 200});
            GuiProgressBar((Rectangle){12,  GetScreenHeight()-12*2, GetScreenWidth()-12*2, 12}, "", "" , &renderProgress,  0.0f, 1.0f);

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
