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
#include <string>
#include <ctime>
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

#define DEFAULT_WIDTH 768
#define DEFUALT_HEIGHT 768

#define SAMPLES_MAX 1024
#define SNAPSHOT 1023


enum GiRendererType {
    RAYTRACE,
    RADIENCE_PROBES,
    RADIENCE_CASCADES
};

enum Mode {
    RUNNING,
    PAUSED,
    RESTART,
    CAPTURE,
    FINISHED
};

typedef struct {
    GiRendererType rendererType;
    Mode mode;
    uint samplesCurr;
    uint samplesMax;
    float timeInitial;
    float timeElapsed;

    int width;
    int height;
    Vector2 mousePos;

    float frameTime; // ms
    float fps; // frames per second
    float rps; // rays per second

    float distClosestSurface;

    uint sceneSdfProgram;
    uint giRayTraceProgram;
    uint giRadienceProbesProgram;
    uint giRadienceCascadesProgram;
    uint sceneNormalsProgram;
    uint postProcessProgram;
    uint finalPassProgram;

    uint sceneColorSSBO;
    uint sceneMaskSSBO;
    uint sceneSdfSSBO;
    uint sceneNormalsSSBO;
    uint sceneGiSSBO_A; // TODO: just accumulate on 1 buffer
    uint sceneGiSSBO_B;
    uint finalPassSSBO;

    Texture outputTex;

    std::map<uint, uint> sampleCurrUniformLocs;
    std::map<uint, uint> resolutionUniformLocs;

    Texture textureOut;
} AppState;

void CreateAppState(AppState *state)
{
    state->rendererType = RAYTRACE;
    state->mode = RUNNING;
    state->samplesCurr = 0;
    state->samplesMax = SAMPLES_MAX;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;

    state->width = DEFAULT_WIDTH;
    state->height = DEFUALT_HEIGHT;

    state->mousePos = Vector2({0, 0});

    state->frameTime = 0;
    state->fps = 0;
    state->distClosestSurface = 0;
}

void CreateRenderPipeline(AppState *state)
{
    /* ----------------------------- Compile Shaders ---------------------------- */
    char *shaderCode;
    uint compiledShader;
    
    // Scene Sdf
    shaderCode = LoadFileText("shaders/scene_sdf.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->sceneSdfProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    // Scene Normals
    shaderCode = LoadFileText("shaders/scene_normals.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->sceneNormalsProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    // Global Illumination
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

    // Post-processing
    shaderCode = LoadFileText("shaders/post_process.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->postProcessProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    // Final Pass
    uint fragShader, vertShader;
    shaderCode = LoadFileText("shaders/final_pass.frag");
    fragShader = rlCompileShader(shaderCode, RL_FRAGMENT_SHADER);
    UnloadFileText(shaderCode);
    shaderCode = LoadFileText("shaders/default.vert");
    vertShader = rlCompileShader(shaderCode, RL_VERTEX_SHADER);
    UnloadFileText(shaderCode);
    state->finalPassProgram = rlLoadShaderProgram(vertShader, fragShader);

    /* -------------------------- Get Uniform Locations ------------------------- */

    auto programs = {
        state->sceneSdfProgram,
        state->giRayTraceProgram,
        state->giRadienceProbesProgram,
        state->giRadienceCascadesProgram,
        state->sceneNormalsProgram,
        state->postProcessProgram,
        state->finalPassProgram
    };

    for(auto prog : programs)
    {
        state->resolutionUniformLocs[prog] = rlGetLocationUniform(prog, "resolution");
    }


    programs = {
        state->giRayTraceProgram, 
        state->giRadienceProbesProgram, 
        state->giRadienceCascadesProgram
    };

    for(auto prog : programs)
    {
        state->sampleCurrUniformLocs[prog] = rlGetLocationUniform(prog, "samplesCurr");
    }

    /* ------------------------------ Create SSBOs ------------------------------ */
    uint size = state->width * state->height;
    state->sceneColorSSBO = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY);
    state->sceneMaskSSBO = rlLoadShaderBuffer(size*sizeof(float), NULL, RL_DYNAMIC_COPY);
    state->sceneSdfSSBO = rlLoadShaderBuffer(size*sizeof(float), NULL, RL_DYNAMIC_COPY);
    state->sceneNormalsSSBO = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY); 
    state->sceneGiSSBO_A = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY); 
    state->sceneGiSSBO_B = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY); 
    state->finalPassSSBO = rlLoadShaderBuffer(size*sizeof(float)*4, NULL, RL_DYNAMIC_COPY);

    DrawTexture(state->outputTex, 0, 0, WHITE);
}

void DeleteRenderPipeline(AppState *state)
{
    rlUnloadShaderBuffer(state->sceneColorSSBO);
    rlUnloadShaderBuffer(state->sceneMaskSSBO);
    rlUnloadShaderBuffer(state->sceneSdfSSBO);
    rlUnloadShaderBuffer(state->sceneNormalsSSBO);
    rlUnloadShaderBuffer(state->sceneGiSSBO_A);
    rlUnloadShaderBuffer(state->sceneGiSSBO_B);
    rlUnloadShaderBuffer(state->finalPassSSBO);

    rlUnloadShaderProgram(state->sceneSdfProgram);
    rlUnloadShaderProgram(state->giRayTraceProgram);
    rlUnloadShaderProgram(state->giRadienceProbesProgram);
    rlUnloadShaderProgram(state->giRadienceCascadesProgram);
    rlUnloadShaderProgram(state->sceneNormalsProgram);
    rlUnloadShaderProgram(state->postProcessProgram);
    rlUnloadShaderProgram(state->finalPassProgram);

    UnloadTexture(state->outputTex);               
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

const char* GetTimeStamp()
{
    time_t now = time(0);
    tm *ltm = localtime(&now);
    return TextFormat("%i-%i-%i-%i-%i-%i", ltm->tm_year, ltm->tm_mon, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
}

void SaveImage(AppState *state)
{
    RenderTexture2D renderTexTarget = LoadRenderTexture(state->width, state->height);
    
    BeginTextureMode(renderTexTarget);
        rlEnableShader(state->finalPassProgram);
            rlBindShaderBuffer(state->finalPassSSBO, 1);
            rlLoadDrawQuad();
            rlDrawCall();
        rlDisableShader();
    EndTextureMode();

    Image img = LoadImageFromTexture(renderTexTarget.texture); 
    ExportImage(img, TextFormat("/snapshots/snapshot_%i_%i_%03i.png",state->samplesCurr, state->samplesMax, GetTimeStamp()));  
    UnloadImage(img);  

    UnloadRenderTexture(renderTexTarget);
}

void RunRenderPipeline(AppState *state)
{
    /* ------------------------------ Set Uniforms ------------------------------ */
    auto programs = {
        state->sceneSdfProgram,
        state->giRayTraceProgram,
        state->giRadienceProbesProgram,
        state->giRadienceCascadesProgram,
        state->sceneNormalsProgram,
        state->postProcessProgram,
        state->finalPassProgram
    };

    int data[2] = {state->width, state->height};
    for(auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->resolutionUniformLocs[prog], data, RL_SHADER_UNIFORM_IVEC2, 1);
        rlDisableShader();
    }

    programs = {
        state->giRayTraceProgram, 
        state->giRadienceProbesProgram, 
        state->giRadienceCascadesProgram
    };
    
    for(auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->sampleCurrUniformLocs[prog], &(state->samplesCurr), SHADER_UNIFORM_UINT, 1);
        rlDisableShader();
    }

    /* ------------------------ Dispatch compute shaders ------------------------ */
    
    // Generate SDF Map
    rlEnableShader(state->sceneSdfProgram);
    rlBindShaderBuffer(state->sceneMaskSSBO, 1);
    rlBindShaderBuffer(state->sceneSdfSSBO, 2);
    rlComputeShaderDispatch(state->width/16, state->height/16, 1);
    rlDisableShader();

    // Compute Normals
    // rlEnableShader(state->sceneNormalsProgram);
    // rlBindShaderBuffer(state->sceneSdfSSBO, 1);
    // rlBindShaderBuffer(state->sceneNormalsSSBO, 2);
    // rlComputeShaderDispatch(state->width/16, state->height/16, 1);
    // rlDisableShader();

    uint giProgramChosen;
    switch (state->rendererType)
    {
    case GiRendererType::RAYTRACE:
        giProgramChosen = state->giRayTraceProgram;
        break;
    case GiRendererType::RADIENCE_PROBES:
        giProgramChosen = state->giRayTraceProgram;
        break;
    case GiRendererType::RADIENCE_CASCADES:
        giProgramChosen = state->giRayTraceProgram;
        break;
    default:
        break;
    }

    // Compute GI
    rlEnableShader(giProgramChosen);
        rlBindShaderBuffer(state->sceneColorSSBO, 1);
        rlBindShaderBuffer(state->sceneSdfSSBO, 2);
        rlBindShaderBuffer(state->sceneNormalsSSBO, 3);
        rlBindShaderBuffer(state->sceneGiSSBO_A, 4);
        rlBindShaderBuffer(state->sceneGiSSBO_B, 5);
        rlComputeShaderDispatch(state->width/16, state->height/16, 1);
    rlDisableShader();
    std::swap(state->sceneGiSSBO_A, state->sceneGiSSBO_B); // ping pong accumalation.

    // Compute Composite and Post-processing
    rlEnableShader(state->postProcessProgram);
        rlBindShaderBuffer(state->sceneColorSSBO, 1);
        rlBindShaderBuffer(state->sceneGiSSBO_A, 2);
        rlBindShaderBuffer(state->finalPassSSBO, 3);
        rlComputeShaderDispatch(state->width/16, state->height/16, 1);
    rlDisableShader();
}

void UpdateFrameBuffer(AppState *state)
{
    rlEnableShader(state->finalPassProgram);
        rlBindShaderBuffer(state->finalPassSSBO, 1);
        rlBindShaderBuffer(state->sceneSdfSSBO, 2);
        rlLoadDrawQuad();
        rlDrawCall();
        // DrawTexture(state->outputTex, 0, 0, WHITE);
    rlDisableShader();
}

void UpdateGui(AppState *state)
{
    if(IsCursorOnScreen())
    {
        rlReadShaderBuffer(state->sceneSdfSSBO, &state->distClosestSurface, sizeof(float), (GetMouseX() + GetMouseY() * state->width) * sizeof(float));
        u_char distNorm = state->distClosestSurface/((float)state->width*0.5) * 255;
        DrawCircleLines(GetMouseX(), GetMouseY(), state->distClosestSurface, RED);
        DrawCircle(GetMouseX(), GetMouseY(), 2, {distNorm, distNorm, distNorm, 255});
    }

    Vector2 anchor = Vector2({0,0});
    DrawRectangle(0, 0, 314, 90, {0, 0, 0, 200});
    DrawText(TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription), 10, 10, 10, Color({255, 255, 255, 100}));
    DrawText(TextFormat("%s %d %d", "MouseXY", GetMouseX(), GetMouseY()), 10, 30, 10, Color({255, 255, 255, 255}));

    DrawText(TextFormat("%s %f", "Distance To Closest Surface", state->distClosestSurface), 10, 50, 10, Color({255, 255, 255, 255}));

    DrawText(TextFormat("%s %d", "FPS", GetFPS()), 10, 70, 10, Color({255, 255, 255, 255}));

    // DrawRectangle(0, GetScreenHeight()-12, GetScreenWidth(), 12, {0, 0, 0, 200});

    float render_progress = (float)state->samplesCurr / state->samplesMax;
    GuiProgressBar((Rectangle){12,  GetScreenHeight()-12*2, GetScreenWidth()-12*2, 12}, "", "" , &render_progress,  0.0f, 1.0f);
}

void UpdateInput(AppState *state)
{
    if(IsKeyPressed(KEY_F12))
    {
        state->mode = CAPTURE;
    }

    if(IsKeyPressed(KEY_F5))
    {
        state->mode = RESTART;
    }

    // if(IsKeyPressed(KEY_F1))
    // {
    //     state->rendererType = RAYTRACE;
    // }

    // if(IsKeyPressed(KEY_F2))
    // {
    //     state->rendererType = RADIENCE_PROBES;
    // }

    // if(IsKeyPressed(KEY_F3))
    // {
    //     state->rendererType = RADIENCE_CASCADES;
    // }
}

void UpdateState(AppState *state)
{
    if(state->samplesCurr == state->samplesMax)
    {
        state->mode = FINISHED;
    }

    switch (state->mode)
    {
    case Mode::RUNNING:
        RunRenderPipeline(state);
        state->timeElapsed = GetTime() - state->timeInitial;
        state->samplesCurr++;
    break;

    case Mode::RESTART:
        RestartRenderer(state);
        state->mode = RUNNING;
    break;

    case Mode::PAUSED:
        break;
    
    case Mode::FINISHED:
        break;
    
    case Mode::CAPTURE:
        SaveImage(state);
        state->mode = RUNNING;
        break;
    
    default:
        break;
    }

    state->frameTime = GetFrameTime();
    state->mousePos = GetMousePosition();
    state->fps = GetFPS();
}

void Render(AppState *state)
{
    BeginDrawing();
    ClearBackground(BLANK);
    UpdateFrameBuffer(state);
    UpdateGui(state);
    EndDrawing();
}

AppState state;

int main(void)
{
    CreateAppState(&state);

    InitWindow(state.width, state.height, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));

    SearchAndSetResourceDir("resources");
    GuiLoadStyle("styles/style_darker.rgs");

    HideCursor();

    CreateRenderPipeline(&state);

    Image img = GenImageColor(state.width, state.height, MAGENTA);
    state.outputTex = LoadTextureFromImage(img);
    UnloadImage(img);

    /* -------------------------------- Main Loop ------------------------------- */
    while (!WindowShouldClose())
    {
        UpdateInput(&state);
        UpdateState(&state);
        Render(&state);
    }

    DeleteRenderPipeline(&state);
    CloseWindow();

    return 0;
}

