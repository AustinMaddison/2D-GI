#define TOOL_NAME "2D Global Illumination"
#define TOOL_SHORT_NAME "2D-GI"
#define TOOL_VERSION "2.0"
#define TOOL_DESCRIPTION "GPU Benchmark Render Engine"
#define TOOL_DESCRIPTION_BREAK ""
#define TOOL_RELEASE_DATE "Month.Date"
#define TOOL_LOGO_COLOR 0x7da9b9ff

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

#define DEFAULT_WIDTH 1024
#define DEFUALT_HEIGHT 1024
#define COMPUTE_SHADER_DISPATCH_X 16
#define COMPUTE_SHADER_DISPATCH_Y 16

#define DEFAULT_SAMPLES_MAX 4096

enum GiRendererType
{
    RAYTRACE,
    RADIENCE_PROBES,
    RADIENCE_CASCADES
};

enum Mode
{
    RUNNING,
    PAUSED,
    RESTART,
    CAPTURE,
    FINISHED
};

typedef struct
{
    GiRendererType rendererType;
    Mode mode;
    uint samplesCurr;
    uint samplesMax;
    double timeInitial;
    double timeElapsed;

    bool isSceneChanged;

    int width;
    int height;
    Vector2 mousePos;

    float frameTime; // ms
    float fps;       // frames per second
    float rps;       // rays per second

    float distClosestSurface;
    Vector2 normals;

    uint jfaSetSeedProgram;   // jump flood
    uint jfaIterationProgram; // jump flood
    uint sceneSdfProgram;
    uint giRayTraceProgram;
    uint giRadienceProbesProgram;
    uint giRadienceCascadesProgram;
    uint sceneNormalsProgram;
    uint postProcessProgram;
    uint finalPassProgram;

    uint sceneColorMaskSSBO;
    uint jfaSSBO_A; // jump flood
    uint jfaSSBO_B; // jump flood
    uint sceneSdfSSBO;
    uint sceneNormalsSSBO;
    uint sceneGiSSBO_A; // TODO: just accumulate on 1 buffer
    uint sceneGiSSBO_B;
    uint finalPassSSBO;

    uint sceneSdfTex;
    uint sceneNormalsTex;

    Texture2D sceneColorMaskTex;

    std::map<uint, uint> sampleCurrUniformLocs;
    std::map<uint, uint> resolutionUniformLocs;
    std::map<uint, uint> mouseUniformLocs;
} AppState;

void CreateAppState(AppState *state)
{
    state->rendererType = RAYTRACE;
    state->mode = RUNNING;
    state->samplesCurr = 0;
    state->samplesMax = DEFAULT_SAMPLES_MAX;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;

    state->isSceneChanged = true;

    state->width = DEFAULT_WIDTH;
    state->height = DEFUALT_HEIGHT;

    state->mousePos = Vector2({0, 0});

    state->frameTime = 0;
    state->fps = 0;
    state->distClosestSurface = 0;
    state->normals = Vector2({0, 0});
}

void CreateRenderPipeline(AppState *state)
{
    /* ----------------------------- Compile Shaders ---------------------------- */
    char *shaderCode;
    uint compiledShader;

    // Jump Flood Algorithm
    shaderCode = LoadFileText("shaders/jump_flood_set_seed.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->jfaSetSeedProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    shaderCode = LoadFileText("shaders/jump_flood_iteration.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->jfaIterationProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

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
    // shaderCode = LoadFileText("shaders/gi_raytrace_debug2.glsl");
    // shaderCode = LoadFileText("shaders/gi_raytrace_debug.glsl");
    // shaderCode = LoadFileText("shaders/gi_raytrace_debug_bounce.glsl");
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

    auto programs = 
    {
        state->jfaSetSeedProgram,
        state->jfaIterationProgram,
        state->sceneSdfProgram,
        state->sceneSdfProgram,
        state->giRayTraceProgram,
        state->giRadienceProbesProgram,
        state->giRadienceCascadesProgram,
        state->sceneNormalsProgram,
        state->postProcessProgram,
        state->finalPassProgram
    };

    for (auto prog : programs)
    {
        state->resolutionUniformLocs[prog] = rlGetLocationUniform(prog, "resolution");
    }

    programs = {
        state->giRayTraceProgram,
    };

    for (auto prog : programs)
    {
        state->mouseUniformLocs[prog] = rlGetLocationUniform(prog, "mouse");
    }

    programs = {
        state->giRayTraceProgram,
        state->giRadienceProbesProgram,
        state->giRadienceCascadesProgram};

    for (auto prog : programs)
    {
        state->sampleCurrUniformLocs[prog] = rlGetLocationUniform(prog, "samplesCurr");
    }

    /* ----------------------------- Create Textures ---------------------------- */

    // Load images into OpenGl Textures
    Image sceneImg = LoadImage("textures/test_flood_fill.png");
    // Image sceneImg = LoadImage("textures/test_flood_fill_box.png");
    ImageFormat(&sceneImg, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);

    state->sceneColorMaskTex = LoadTextureFromImage(sceneImg);

    state->sceneSdfTex = rlLoadTexture(NULL, state->width, state->height, RL_PIXELFORMAT_UNCOMPRESSED_R32, 1);
    state->sceneNormalsTex = rlLoadTexture(NULL, state->width, state->height, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32, 1);
    
    /* ------------------------------ Create SSBOs ------------------------------ */

    uint size = state->width * state->height;
    state->jfaSSBO_A = rlLoadShaderBuffer(size * sizeof(float) * 2, NULL, RL_DYNAMIC_COPY);
    state->jfaSSBO_B = rlLoadShaderBuffer(size * sizeof(float) * 2, NULL, RL_DYNAMIC_COPY);
    state->sceneColorMaskSSBO = rlLoadShaderBuffer(size * sizeof(float) * 4, sceneImg.data, RL_DYNAMIC_COPY);
    state->sceneSdfSSBO = rlLoadShaderBuffer(size * sizeof(float), NULL, RL_DYNAMIC_COPY);
    state->sceneNormalsSSBO = rlLoadShaderBuffer(size * sizeof(float) * 2, NULL, RL_DYNAMIC_COPY);
    state->sceneGiSSBO_A = rlLoadShaderBuffer(size * sizeof(float) * 4, NULL, RL_DYNAMIC_COPY);
    state->sceneGiSSBO_B = rlLoadShaderBuffer(size * sizeof(float) * 4, NULL, RL_DYNAMIC_COPY);
    state->finalPassSSBO = rlLoadShaderBuffer(size * sizeof(float) * 4, NULL, RL_DYNAMIC_COPY);
}

void DeleteRenderPipeline(AppState *state)
{
    // Buffers
    rlUnloadShaderBuffer(state->sceneColorMaskSSBO);
    rlUnloadShaderBuffer(state->jfaSSBO_A);
    rlUnloadShaderBuffer(state->jfaSSBO_B);
    rlUnloadShaderBuffer(state->sceneSdfSSBO);
    rlUnloadShaderBuffer(state->sceneNormalsSSBO);
    rlUnloadShaderBuffer(state->sceneGiSSBO_A);
    rlUnloadShaderBuffer(state->sceneGiSSBO_B);
    rlUnloadShaderBuffer(state->finalPassSSBO);

    // Textures
    rlUnloadTexture(state->sceneSdfTex);
    rlUnloadTexture(state->sceneNormalsTex);

    // Buffer
    rlUnloadShaderProgram(state->jfaSetSeedProgram);
    rlUnloadShaderProgram(state->jfaIterationProgram);
    rlUnloadShaderProgram(state->sceneSdfProgram);
    rlUnloadShaderProgram(state->giRayTraceProgram);
    rlUnloadShaderProgram(state->giRadienceProbesProgram);
    rlUnloadShaderProgram(state->giRadienceCascadesProgram);
    rlUnloadShaderProgram(state->sceneNormalsProgram);
    rlUnloadShaderProgram(state->postProcessProgram);
    rlUnloadShaderProgram(state->finalPassProgram);
}

void RestartRenderer(AppState *state)
{
    state->samplesCurr = 0;
    state->samplesMax = state->samplesMax;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;

    DeleteRenderPipeline(state);
    CreateRenderPipeline(state);
}

const char *GetTimeStamp()
{
    time_t now = time(0);
    tm *ltm = localtime(&now);
    return TextFormat("%i-%i-%i-%i-%i-%i", ltm->tm_year, ltm->tm_mon, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
}

void DispatchJumpFlood(AppState *state)
{
    /* -------------------------------- Set Seed -------------------------------- */
    rlEnableShader(state->jfaSetSeedProgram);
    rlBindShaderBuffer(state->sceneColorMaskSSBO, 1);
    rlBindShaderBuffer(state->jfaSSBO_A, 2);
    rlComputeShaderDispatch(state->width / COMPUTE_SHADER_DISPATCH_X, state->height / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();
    
    /* --------------------------- Iterate Jump Flood --------------------------- */
    int stepWidth = std::max(state->width, state->height)/2;
    // int stepWidth = 512;
    // int iterations = static_cast<int>(std::log2(stepWidth));
    int iterations = 9;
    uint stepWidthLocs =  rlGetLocationUniform(state->jfaIterationProgram, "stepWidth");
    
    rlEnableShader(state->jfaIterationProgram);

    for (int i = 0; i < (int)iterations; i++)
    {
        rlSetUniform(stepWidthLocs, &stepWidth, RL_SHADER_UNIFORM_INT, 1);
        rlBindShaderBuffer(state->jfaSSBO_A, 1);
        rlBindShaderBuffer(state->jfaSSBO_B, 2);
        rlComputeShaderDispatch(state->width / COMPUTE_SHADER_DISPATCH_X, state->height / COMPUTE_SHADER_DISPATCH_Y, 1);
        std::swap(state->jfaSSBO_A, state->jfaSSBO_B);
        stepWidth /=2;
    }
    rlDisableShader();
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
    ImageFlipVertical(&img);

    auto filename = TextFormat("snapshot_%i_%i_%s.png", state->samplesCurr, state->samplesMax, GetTimeStamp());
    ExportImage(img, filename);
    UnloadImage(img);

    TraceLog(LOG_INFO, "Image saved to %s", filename);
    UnloadRenderTexture(renderTexTarget);
}

void SetUniforms(const std::vector<uint> &programs, const std::map<uint, uint> &uniformLocs, const void *data, rlShaderUniformDataType uniformType, int count)
{
    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(uniformLocs.at(prog), data, uniformType, count);
        rlDisableShader();
    }
}

void RunRenderPipeline(AppState *state)
{
    /* ------------------------------ Set Uniforms ------------------------------ */
    auto programs = {
        state->jfaSetSeedProgram,
        state->jfaIterationProgram,
        state->sceneSdfProgram,
        state->giRayTraceProgram,
        state->giRadienceProbesProgram,
        state->giRadienceCascadesProgram,
        state->sceneNormalsProgram,
        state->postProcessProgram,
        state->finalPassProgram
    };

    int data[2] = {state->width, state->height};
    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->resolutionUniformLocs[prog], data, RL_SHADER_UNIFORM_IVEC2, 1);
        rlDisableShader();
    }

    programs = {
        state->giRayTraceProgram};

    int data2[2] = {state->mousePos.x, state->mousePos.y};
    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->mouseUniformLocs[prog], data2, RL_SHADER_UNIFORM_IVEC2, 1);
        rlDisableShader();
    }

    programs = {
        state->giRayTraceProgram,
        state->giRadienceProbesProgram,
        state->giRadienceCascadesProgram};

    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->sampleCurrUniformLocs[prog], &(state->samplesCurr), SHADER_UNIFORM_UINT, 1);
        rlDisableShader();
    }

    /* ------------------------ Dispatch compute shaders ------------------------ */

    if (state->isSceneChanged)
    {
        // Compute Jump Flood
        // DispatchJumpFlood(state);

        // Generate SDF Map
        rlEnableShader(state->sceneSdfProgram);
        rlActiveTextureSlot(1);

        rlSetTexture(state->sceneColorMaskTex.id);
        rlBindImageTexture(state->sceneSdfTex, 2, RL_PIXELFORMAT_UNCOMPRESSED_R32, false);

        rlComputeShaderDispatch(state->width / COMPUTE_SHADER_DISPATCH_X, state->height / COMPUTE_SHADER_DISPATCH_Y, 1);
        rlDisableShader();

        // Compute Normals
        rlEnableShader(state->sceneNormalsProgram);

        rlActiveTextureSlot(1);
        rlSetTexture(state->sceneSdfTex);
        rlBindImageTexture(state->sceneNormalsTex, 2, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32, false);


        rlBindShaderBuffer(state->sceneSdfSSBO, 1);
        rlBindShaderBuffer(state->sceneNormalsSSBO, 2);
        rlComputeShaderDispatch(state->width / COMPUTE_SHADER_DISPATCH_X, state->height / COMPUTE_SHADER_DISPATCH_Y, 1);
        rlDisableShader();

        state->isSceneChanged = false;
    }

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
    rlBindShaderBuffer(state->sceneColorMaskSSBO, 1);
    
    rlEnableTexture(state->sceneSdfTex);
    rlActiveTextureSlot(2);
    

    rlBindShaderBuffer(state->sceneSdfSSBO, 2);
    rlBindShaderBuffer(state->sceneNormalsSSBO, 3);
    rlBindShaderBuffer(state->sceneGiSSBO_A, 4);
    rlBindShaderBuffer(state->sceneGiSSBO_B, 5);
    rlComputeShaderDispatch(state->width / COMPUTE_SHADER_DISPATCH_X, state->height / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();
    std::swap(state->sceneGiSSBO_A, state->sceneGiSSBO_B); // ping pong accumalation.

    // Compute Composite and Post-processing
    rlEnableShader(state->postProcessProgram);
    rlBindShaderBuffer(state->sceneColorMaskSSBO, 1);
    rlBindShaderBuffer(state->sceneGiSSBO_A, 2);
    rlBindShaderBuffer(state->finalPassSSBO, 3);
    rlComputeShaderDispatch(state->width / COMPUTE_SHADER_DISPATCH_X, state->height / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();
}

void UpdateFrameBuffer(AppState *state)
{
    rlEnableShader(state->finalPassProgram);
    rlBindShaderBuffer(state->finalPassSSBO, 1);
    rlBindShaderBuffer(state->sceneSdfSSBO, 2);
    rlBindShaderBuffer(state->sceneColorMaskSSBO, 3);
    rlBindShaderBuffer(state->jfaSSBO_A, 4);
    rlLoadDrawQuad();
    rlDrawCall();
    rlDisableShader();
}

void UpdateGui(AppState *state)
{
    if (IsCursorOnScreen())
    {
        rlReadShaderBuffer(state->sceneSdfSSBO, &state->distClosestSurface, sizeof(float), (GetMouseX() + GetMouseY() * state->width) * sizeof(float));

        rlReadShaderBuffer(state->sceneNormalsSSBO, (float *)&state->normals, sizeof(float) * 2, (GetMouseX() + GetMouseY() * state->width) * sizeof(float));

        u_char distNorm = state->distClosestSurface / ((float)state->width * 0.5) * 255;
        DrawCircleLines(GetMouseX(), GetMouseY(), state->distClosestSurface, RED);
        DrawCircle(GetMouseX(), GetMouseY(), 2, {distNorm, distNorm, distNorm, 255});
    }
    Vector2 anchor = Vector2({10, 10});

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 100);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 100);
    GuiWindowBox((Rectangle){anchor.x - 10, anchor.y - 10, 314, 200}, "Info Panel");
    anchor.y += 20;
    GuiLabel((Rectangle){anchor.x, anchor.y, 300, 20}, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));
    GuiLabel((Rectangle){anchor.x, anchor.y + 20, 300, 20}, TextFormat("%s %d %d", "MouseXY", GetMouseX(), GetMouseY()));
    GuiLabel((Rectangle){anchor.x, anchor.y + 40, 300, 20}, TextFormat("%s %f, %s %f %f", "D", state->distClosestSurface, "N", state->normals.x, state->normals.y));
    GuiLabel((Rectangle){anchor.x, anchor.y + 60, 300, 20}, TextFormat("%s %d", "FPS", GetFPS()));
    GuiLabel((Rectangle){anchor.x, anchor.y + 80, 300, 20}, TextFormat("Frame Time: %.3f ms", state->frameTime));
    GuiLabel((Rectangle){anchor.x, anchor.y + 100, 300, 20}, TextFormat("Render Time: %.2f", state->timeElapsed));
    GuiLabel((Rectangle){anchor.x, anchor.y + 120, 300, 20}, TextFormat("Samples: %d / %d", state->samplesCurr, state->samplesMax));
    GuiLabel((Rectangle){anchor.x, anchor.y + 140, 300, 20}, TextFormat("Renderer: %s", state->rendererType == RAYTRACE ? "Raytrace" : state->rendererType == RADIENCE_PROBES ? "Radiance Probes"
                                                                                                                                                                              : "Radiance Cascades"));

    // DrawRectangle(0, GetScreenHeight()-12, GetScreenWidth(), 12, {0, 0, 0, 200});

    float render_progress = (float)state->samplesCurr / state->samplesMax;
    GuiProgressBar((Rectangle){12, GetScreenHeight() - 12 * 2, GetScreenWidth() - 12 * 2, 12}, "", "", &render_progress, 0.0f, 1.0f);
}

void UpdateInput(AppState *state)
{
    if (IsKeyPressed(KEY_F12))
    {
        state->mode = CAPTURE;
    }

    if (IsKeyPressed(KEY_F5))
    {
        state->mode = RESTART;
    }

    if (IsKeyPressed(KEY_F4))
    {
        if (state->mode == PAUSED)
            state->mode = RUNNING;
        else
            state->mode = PAUSED;
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
    switch (state->mode)
    {
    case Mode::RUNNING:
        RunRenderPipeline(state);
        state->timeElapsed = GetTime() - state->timeInitial;
        state->samplesCurr++;
        break;

    case Mode::RESTART:
        TraceLog(LOG_INFO, "Renderer restarted");
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

    if (state->samplesCurr == state->samplesMax)
    {
        state->mode = FINISHED;
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
