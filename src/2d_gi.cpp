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

#include "glad.h" // to use memory synchronization mem barrier

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

#define DEFAULT_RESOURCE_FOLDER "resources"
#define DEFAULT_GUI_STYLE "styles/style_darker.rgs"
#define DEFAULT_CONFIG_FLAGS FLAG_WINDOW_RESIZABLE
#define DEFAULT_WIDTH 512
#define DEFAULT_HEIGHT 512
#define DEFAULT_REFRESH_RATE 60
#define COMPUTE_SHADER_DISPATCH_X 16
#define COMPUTE_SHADER_DISPATCH_Y 16

#define DEFAULT_PROBE_WIDTH 32
#define DEFAULT_PROBE_HEIGHT 32
#define DEFAULT_PROBE_ANGLE_RESOLUTION 16

#define DEFAULT_SAMPLES_MAX 4096

enum GiRendererType
{
    RAYTRACE,
    IRRADIENCE_PROBES,
};

enum Mode
{
    RUNNING,
    PAUSED,
    RESTART,
    CAPTURE,
    FINISHED
};

enum FrambufferOutputType
{
    FINAL,
    DEBUG_COLOR,
    DEBUG_MASK,
    DEBUG_COLOR_MASK,
    DEBUG_JFA,
    DEBUG_SDF,
    DEBUG_NORMAL,
    DEBUG_GI,
    DEBUG_COMPOSITE
};

typedef struct
{
    /* ---------------------------------- State --------------------------------- */
    GiRendererType rendererType;
    FrambufferOutputType oututType;
    Mode mode;
    uint samplesCurr;
    uint samplesMax;
    double timeInitial;
    double timeElapsed;

    bool isDirty;

    Vector2 windowSize;
    Vector2 sceneSize;

    bool isPanning;
    Vector2 mousePosCurr;
    Vector2 mousePosPrev;
    Vector2 cameraPos;
    float cameraZoom;

    /* --------------------------------- Metrics -------------------------------- */
    float frameTime; // ms
    float fps;       // frames per second
    float rps;       // rays per second
    uint rayCount;

    float distClosestSurface;
    Vector2 normals;

    /* --------------------------------- Shaders -------------------------------- */

    uint jfaSetSeedProgram;   // jump flood
    uint jfaIterationProgram; // jump flood
    uint sceneSdfProgram;

    // Monte Carlo Raytracing
    uint giRayTraceProgram;

    // Irradiance Probes
    uint giIrradianceProbeTraceProgram;
    uint giIrradianceProbeQueryProgram;

    uint sceneNormalsProgram;
    uint postProcessProgram;
    uint finalPassProgram;

    /* -------------------------------- Textures -------------------------------- */
    uint sceneColorMaskTex;
    uint jfaTex_A;
    uint jfaTex_B;
    uint sceneSdfTex;
    uint sceneNormalsTex;
    uint giProbeIrradianceDepthTex;

    /* ---------------------------------- SBBOs --------------------------------- */

    uint rayCountSSBO;  // TODO: add to renderer.
    uint sceneGiSSBO_A; // TODO: just accumulate on 1 buffer
    uint sceneGiSSBO_B;
    uint finalPassSSBO;

    /* ------------------------------ Uniform Maps ------------------------------ */
    std::map<uint, uint> sampleCurrUniformLocs;
    std::map<uint, uint> resolutionUniformLocs;
    std::map<uint, uint> mouseUniformLocs;
    std::map<uint, uint> modelUnifromLocs;
    std::map<uint, uint> timeUniformLocs;
} AppState;

void CreateAppState(AppState *state)
{
    state->rendererType = GiRendererType::IRRADIENCE_PROBES;
    state->mode = RUNNING;
    state->samplesCurr = 0;
    state->samplesMax = DEFAULT_SAMPLES_MAX;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;

    state->isDirty = true;

    state->windowSize = Vector2({DEFAULT_WIDTH, DEFAULT_HEIGHT});

    state->sceneSize = Vector2({DEFAULT_WIDTH, DEFAULT_HEIGHT});

    state->isPanning = false;
    state->mousePosCurr = Vector2Zero();
    state->mousePosPrev = Vector2Zero();

    state->cameraPos = Vector2Zero();
    state->cameraZoom = 1.0;

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
    // SECTION - MonteCarlo Rayracing
    // shaderCode = LoadFileText("shaders/gi_raytrace_debug2.glsl");
    // shaderCode = LoadFileText("shaders/gi_raytrace_debug.glsl");
    // shaderCode = LoadFileText("shaders/gi_raytrace_debug_bounce.glsl");
    shaderCode = LoadFileText("shaders/gi_raytrace.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->giRayTraceProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    // SECTION - Irradiance Probes
    shaderCode = LoadFileText("shaders/gi_radiance_probe_raytrace.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->giIrradianceProbeTraceProgram = rlLoadComputeShaderProgram(compiledShader);
    UnloadFileText(shaderCode);

    shaderCode = LoadFileText("shaders/gi_radiance_probe_query.glsl");
    compiledShader = rlCompileShader(shaderCode, RL_COMPUTE_SHADER);
    state->giIrradianceProbeQueryProgram = rlLoadComputeShaderProgram(compiledShader);
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
            state->giIrradianceProbeTraceProgram,
            state->giIrradianceProbeQueryProgram,
            state->sceneNormalsProgram,
            state->postProcessProgram,
            state->finalPassProgram};

    for (auto prog : programs)
    {
        state->resolutionUniformLocs[prog] = rlGetLocationUniform(prog, "uResolution");
    }

    programs = {
        state->giRayTraceProgram,
    };

    for (auto prog : programs)
    {
        state->mouseUniformLocs[prog] = rlGetLocationUniform(prog, "uMousePos");
    }

    programs = {
        state->finalPassProgram,
        state->giRayTraceProgram,
        state->giIrradianceProbeTraceProgram,
        state->giIrradianceProbeQueryProgram};

    for (auto prog : programs)
    {
        state->sampleCurrUniformLocs[prog] = rlGetLocationUniform(prog, "uSamples");
        state->timeUniformLocs[prog] = rlGetLocationUniform(prog, "uTime");
    }

    programs = {
        state->finalPassProgram,
    };

    for (auto prog : programs)
    {
        state->mouseUniformLocs[prog] = rlGetLocationUniform(prog, "uModel");
    }

    /* ----------------------------- Create Textures ---------------------------- */

    // Load images into OpenGl Textures
    // Image sceneImg = LoadImage("textures/test_flood_fill_box.png");
    // Image sceneImg = LoadImage("textures/test_flood_fill.png");
    Image sceneImg = LoadImage("textures/bitmap.png");
    ImageFlipVertical(&sceneImg);
    ImageFormat(&sceneImg, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);

    state->sceneSize = Vector2({(float)sceneImg.width, (float)sceneImg.height});

    state->sceneColorMaskTex = rlLoadTexture(sceneImg.data, state->sceneSize.x, state->sceneSize.y, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    state->jfaTex_A = rlLoadTexture(NULL, state->sceneSize.x, state->sceneSize.y, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    state->jfaTex_B = rlLoadTexture(NULL, state->sceneSize.x, state->sceneSize.y, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    state->sceneSdfTex = rlLoadTexture(NULL, state->sceneSize.x, state->sceneSize.y, RL_PIXELFORMAT_UNCOMPRESSED_R32, 1);
    state->sceneNormalsTex = rlLoadTexture(NULL, state->sceneSize.x, state->sceneSize.y, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);

    auto textures = {
        state->sceneColorMaskTex,
        state->jfaTex_A,
        state->jfaTex_B,
        state->sceneSdfTex,
        state->sceneNormalsTex};

    for (auto tex : textures)
    {
        rlTextureParameters(tex, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
        rlTextureParameters(tex, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        rlTextureParameters(tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_BILINEAR);
        rlTextureParameters(tex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_BILINEAR);
    }

    // SECTION - Irradiance Probe Texture
    uint probeTextureSize = sqrt((DEFAULT_PROBE_WIDTH * DEFAULT_PROBE_HEIGHT) * DEFAULT_PROBE_ANGLE_RESOLUTION);
    state->giProbeIrradianceDepthTex = rlLoadTexture(NULL, probeTextureSize, probeTextureSize, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);

    textures = {
        state->giProbeIrradianceDepthTex
    };

    for (auto tex : textures)
    {
        rlTextureParameters(tex, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
        rlTextureParameters(tex, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
        rlTextureParameters(tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_BILINEAR);
        rlTextureParameters(tex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_BILINEAR);
    }

    /* ------------------------------ Create SSBOs ------------------------------ */

    uint size = state->sceneSize.x * state->sceneSize.y;
    state->rayCountSSBO = rlLoadShaderBuffer(sizeof(GLuint), NULL, RL_DYNAMIC_COPY);
    uint zero = 0;
    rlUpdateShaderBuffer(state->rayCountSSBO, &zero, sizeof(GLuint), 0);

    state->sceneGiSSBO_A = rlLoadShaderBuffer(size * sizeof(float) * 4, NULL, RL_DYNAMIC_COPY);
    state->sceneGiSSBO_B = rlLoadShaderBuffer(size * sizeof(float) * 4, NULL, RL_DYNAMIC_COPY);
    state->finalPassSSBO = rlLoadShaderBuffer(size * sizeof(float) * 4, NULL, RL_DYNAMIC_COPY);
}

void DeleteRenderPipeline(AppState *state)
{
    auto ids = {
        state->jfaSetSeedProgram,
        state->jfaIterationProgram,
        state->sceneSdfProgram,
        state->giRayTraceProgram,
        state->giIrradianceProbeTraceProgram,
        state->giIrradianceProbeQueryProgram,
        state->sceneNormalsProgram,
        state->postProcessProgram,
        state->finalPassProgram};

    for (auto id : ids)
    {
        rlUnloadShaderProgram(id);
    }

    ids = {
        state->sceneGiSSBO_A,
        state->sceneGiSSBO_B,
        state->finalPassSSBO,
        state->rayCountSSBO
    };

    for (auto id : ids)
    {
        rlUnloadShaderBuffer(id);
    }

    ids = {
        state->sceneColorMaskTex,
        state->jfaTex_A,
        state->jfaTex_B,
        state->sceneSdfTex,
        state->sceneNormalsTex,
        state->giProbeIrradianceDepthTex
    };

    for (auto id : ids)
    {
        rlUnloadTexture(id);
    }
}

void RestartRenderer(AppState *state)
{
    state->samplesCurr = 0;
    state->samplesMax = state->samplesMax;
    state->timeInitial = GetTime();
    state->timeElapsed = 0;
    state->isDirty = true;

    DeleteRenderPipeline(state);
    CreateRenderPipeline(state);
}

const char *GetTimeStamp()
{
    time_t now = time(0);
    tm *ltm = localtime(&now);
    return TextFormat("%i-%i-%i-%i-%i-%i", ltm->tm_year, ltm->tm_mon, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
}

void SaveImage(AppState *state)
{
    RenderTexture2D renderTexTarget = LoadRenderTexture(state->sceneSize.x, state->sceneSize.y);

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

void RunJumpFloodAlgorithm(AppState *state)
{
    /* -------------------------------- Set Seed -------------------------------- */
    rlEnableShader(state->jfaSetSeedProgram);
    rlBindImageTexture(state->sceneColorMaskTex, 1, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, true);
    rlBindImageTexture(state->jfaTex_A, 2, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, false);
    rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    /* --------------------------- Iterate Jump Flood --------------------------- */

    int stepWidth = std::max(state->sceneSize.x, state->sceneSize.y) / 2;
    int iterations = ceil(log2(std::max(state->sceneSize.x, state->sceneSize.y)));
    uint stepWidthLocs = rlGetLocationUniform(state->jfaIterationProgram, "uStepWidth");

    rlEnableShader(state->jfaIterationProgram);
    for (int i = 0; i < iterations; i++)
    {
        rlActiveTextureSlot(1);
        rlEnableTexture(state->jfaTex_A);
        rlBindImageTexture(state->jfaTex_B, 2, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, false);
        rlSetUniform(stepWidthLocs, &stepWidth, RL_SHADER_UNIFORM_INT, 1);
        rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        std::swap(state->jfaTex_A, state->jfaTex_B);
        stepWidth /= 2;
    }
    rlDisableShader();
}

void UpdateUniforms(AppState *state)
{
    /* ------------------------------ Set Uniforms ------------------------------ */
    auto programs = {
        state->jfaSetSeedProgram,
        state->jfaIterationProgram,
        state->sceneSdfProgram,
        state->giRayTraceProgram,
        state->giIrradianceProbeTraceProgram,
        state->giIrradianceProbeQueryProgram,
        state->sceneNormalsProgram,
        state->postProcessProgram,
        state->finalPassProgram};

    int data[2] = {state->sceneSize.x, state->sceneSize.y};
    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->resolutionUniformLocs[prog], data, RL_SHADER_UNIFORM_IVEC2, 1);
        rlDisableShader();
    }

    programs = {
        state->giRayTraceProgram};

    int data2[2] = {state->mousePosCurr.x, state->mousePosCurr.y};
    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->mouseUniformLocs[prog], data2, RL_SHADER_UNIFORM_IVEC2, 1);
        rlDisableShader();
    }

    programs = {
        state->finalPassProgram,
        state->giRayTraceProgram,
        state->giIrradianceProbeTraceProgram,
        state->giIrradianceProbeQueryProgram};

    for (auto prog : programs)
    {
        rlEnableShader(prog);
        rlSetUniform(state->sampleCurrUniformLocs[prog], &(state->samplesCurr), SHADER_UNIFORM_UINT, 1);

        float timeElapsed = static_cast<float>(state->timeElapsed);
        rlSetUniform(state->timeUniformLocs[prog], &timeElapsed, SHADER_UNIFORM_FLOAT, 1);
        rlDisableShader();
    }
}

void UpdateSceneSDF(AppState *state)
{
    // Compute Jump Flood
    RunJumpFloodAlgorithm(state);

    // Generate SDF Map
    rlEnableShader(state->sceneSdfProgram);
    rlActiveTextureSlot(1);
    rlEnableTexture(state->sceneColorMaskTex);
    rlBindImageTexture(state->sceneSdfTex, 2, RL_PIXELFORMAT_UNCOMPRESSED_R32, false);
    rlActiveTextureSlot(3);
    rlEnableTexture(state->jfaTex_A);
    rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Compute Normals
    rlEnableShader(state->sceneNormalsProgram);
    rlBindImageTexture(state->sceneSdfTex, 1, RL_PIXELFORMAT_UNCOMPRESSED_R32, true);
    rlBindImageTexture(state->sceneNormalsTex, 2, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, false);
    rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void RunGiRayTracingRenderPipeline(AppState *state)
{
    // Compute GI
    rlEnableShader(state->giRayTraceProgram);
    rlActiveTextureSlot(1);
    rlEnableTexture(state->sceneColorMaskTex);
    rlActiveTextureSlot(2);
    rlEnableTexture(state->sceneSdfTex);
    rlActiveTextureSlot(3);
    rlEnableTexture(state->sceneNormalsTex);
    rlBindShaderBuffer(state->rayCountSSBO, 4);
    rlBindShaderBuffer(state->sceneGiSSBO_A, 5);
    rlBindShaderBuffer(state->sceneGiSSBO_B, 6);
    rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();
    std::swap(state->sceneGiSSBO_A, state->sceneGiSSBO_B); // ping pong accumalation.

    // Compute Composite and Post-processing
    rlEnableShader(state->postProcessProgram);
    rlActiveTextureSlot(1);
    rlEnableTexture(state->sceneColorMaskTex);
    rlBindShaderBuffer(state->sceneGiSSBO_A, 2);
    rlBindShaderBuffer(state->finalPassSSBO, 3);
    rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();
}

void RunGiIrradianceProbeRenderPipeline(AppState *state)
{

    // Compute GI
    rlEnableShader(state->giIrradianceProbeTraceProgram);

    rlActiveTextureSlot(1);
    rlEnableTexture(state->sceneColorMaskTex);
    rlActiveTextureSlot(2);
    rlEnableTexture(state->sceneSdfTex);
    rlActiveTextureSlot(3);
    rlEnableTexture(state->sceneNormalsTex);

    rlBindImageTexture(state->giProbeIrradianceDepthTex, 4, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, false);
    rlBindShaderBuffer(state->rayCountSSBO, 5);

    uint dispatchSize = sqrt((DEFAULT_PROBE_WIDTH * DEFAULT_PROBE_HEIGHT) * DEFAULT_PROBE_ANGLE_RESOLUTION) / COMPUTE_SHADER_DISPATCH_X;
    rlComputeShaderDispatch(dispatchSize, dispatchSize, 1);


    rlDisableShader();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    std::swap(state->sceneGiSSBO_A, state->sceneGiSSBO_B); // ping pong accumalation.



    // Compute Composite and Post-processing
    rlEnableShader(state->postProcessProgram);
    rlActiveTextureSlot(1);
    rlEnableTexture(state->sceneColorMaskTex);
    rlBindShaderBuffer(state->sceneGiSSBO_A, 2);
    rlBindShaderBuffer(state->finalPassSSBO, 3);
    rlComputeShaderDispatch(state->sceneSize.x / COMPUTE_SHADER_DISPATCH_X, state->sceneSize.y / COMPUTE_SHADER_DISPATCH_Y, 1);
    rlDisableShader();

}

void RunRenderPipeline(AppState *state)
{
    UpdateUniforms(state);

    // Scene Representation
    if (state->isDirty)
    {
        UpdateSceneSDF(state);
        state->isDirty = false;
    }

    // Run GI
    switch (state->rendererType)
    {
    case GiRendererType::RAYTRACE:
        RunGiRayTracingRenderPipeline(state);
        break;
    case GiRendererType::IRRADIENCE_PROBES:
        RunGiIrradianceProbeRenderPipeline(state);
        break;
    default:
        break;
    }
}

void UpdateFrameBuffer(AppState *state)
{
    // set camera
    float resFract = state->windowSize.x / state->windowSize.y;
    Matrix posMat = MatrixTranslate(state->cameraPos.x, state->cameraPos.y, 0.0);
    Matrix zoomScaleMat = MatrixScale(state->cameraZoom, state->cameraZoom, 1.0);
    Matrix resFractScaleMat = MatrixScale(1.0, resFract, 1.0);
    Matrix scaleMat = MatrixMultiply(zoomScaleMat, resFractScaleMat);
    Matrix modelMat = MatrixMultiply(posMat, scaleMat);

    rlEnableShader(state->finalPassProgram);
    rlSetUniformMatrix(state->modelUnifromLocs[state->finalPassProgram], modelMat);

    rlBindShaderBuffer(state->finalPassSSBO, 1);

    // Attach other textures for debugging
    rlActiveTextureSlot(2);
    rlEnableTexture(state->sceneColorMaskTex);
    rlActiveTextureSlot(3);
    rlEnableTexture(state->sceneSdfTex);
    rlActiveTextureSlot(4);
    rlEnableTexture(state->sceneNormalsTex);
    rlActiveTextureSlot(5);
    rlEnableTexture(state->jfaTex_A);
    rlActiveTextureSlot(6);
    rlEnableTexture(state->giProbeIrradianceDepthTex);

    rlLoadDrawQuad();
    rlDrawCall();
    rlDisableShader();
}

void DrawMouseInfo(AppState *state)
{
    if (IsCursorOnScreen())
    {
        DrawCircleLines(GetMouseX(), GetMouseY(), state->distClosestSurface, RED);
        DrawCircle(GetMouseX(), GetMouseY(), 2, RED);
    }
}

void DrawInfoPanel(AppState *state)
{
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 100);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 100);
    Vector2 anchor = Vector2({10, 10});
    GuiWindowBox((Rectangle){anchor.x - 10, anchor.y - 10, 200, 180}, "Info Panel");
    anchor.y += 20;
    GuiLabel((Rectangle){anchor.x, anchor.y + 20 - 20, 300, 20}, TextFormat("%s %d %d", "MouseXY", (int)state->mousePosCurr.x, (int)state->mousePosCurr.y));
    GuiLabel((Rectangle){anchor.x, anchor.y + 40 - 20, 300, 20}, TextFormat("%s %f, %s %.2f %.2f", "D", state->distClosestSurface, "N", state->normals.x, state->normals.y));
    GuiLabel((Rectangle){anchor.x, anchor.y + 60 - 20, 300, 20}, TextFormat("%s %d", "FPS", GetFPS()));
    GuiLabel((Rectangle){anchor.x, anchor.y + 80 - 20, 300, 20}, TextFormat("Frame Time: %.3f ms", state->frameTime));
    GuiLabel((Rectangle){anchor.x, anchor.y + 100 - 20, 300, 20}, TextFormat("Render Time: %.2f", state->timeElapsed));
    GuiLabel((Rectangle){anchor.x, anchor.y + 120 - 20, 300, 20}, TextFormat("Samples: %d / %d", state->samplesCurr, state->samplesMax));
    GuiLabel((Rectangle){anchor.x, anchor.y + 140 - 20, 300, 20}, TextFormat("Renderer: %s", state->rendererType == RAYTRACE ? "Raytrace" : "Radiance Cascades"));
}

void DrawProgressBar(AppState *state)
{
    float render_progress = (float)state->samplesCurr / state->samplesMax;
    GuiProgressBar((Rectangle){12, GetScreenHeight() - 12 * 2, GetScreenWidth() - 12 * 2, 12}, "", "", &render_progress, 0.0f, 1.0f);
}

void UpdateGui(AppState *state)
{
    // DrawMouseInfo(state);
    DrawInfoPanel(state);
    DrawProgressBar(state);
}


void UpdateInput(AppState *state)
{
    std::swap(state->mousePosCurr, state->mousePosPrev);
    state->mousePosCurr = Vector2({GetMousePosition().x, static_cast<float>(state->windowSize.y) - GetMousePosition().y});

    if (IsWindowResized())
    {
        state->windowSize = Vector2({(float)GetScreenWidth(), (float)GetScreenHeight()});
    }

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


    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        state->isPanning = true;
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
    {
        state->isPanning = false;
    }

    if (GetMouseWheelMove() != 0)
    {
        float zoomFactor = 1.1f;
        float newZoom = state->cameraZoom * abs(GetMouseWheelMove()) * (GetMouseWheelMove() > 0 ? zoomFactor : 1.0 / zoomFactor);
        state->cameraZoom = newZoom;
    }

    if (state->isPanning == true)
    {
        Vector2 mousePosDelta = state->mousePosCurr - state->mousePosPrev;
        float resFract = state->windowSize.x / state->windowSize.y;

        state->cameraPos += ((mousePosDelta / Vector2({1.0, resFract})) / (state->windowSize / 2.0)) / state->cameraZoom;
    }

    if (IsKeyPressed(KEY_F1))
    {
        state->rendererType = RAYTRACE;
        state->mode = RESTART;
        TraceLog(LOG_INFO, "Switched To Raytraced GI");
    }

    if (IsKeyPressed(KEY_F2))
    {
        state->rendererType = IRRADIENCE_PROBES;
        state->mode = RESTART;
        TraceLog(LOG_INFO, "Switched To Irradiance Field Probe GI");
    }
}

void UpdateMouseCursorIcon(AppState *state)
{

    if(state->isPanning)
    {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
    }
    else if (GetMouseWheelMove() != 0)
    {
        // SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
    }
    else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
}

void UpdateRenderState(AppState *state)
{
    switch (state->mode)
    {
    case Mode::RUNNING:
        // SetTargetFPS(0);
        RunRenderPipeline(state);
        state->timeElapsed = GetTime() - state->timeInitial;
        state->samplesCurr++;
        break;

    case Mode::RESTART:
        TraceLog(LOG_INFO, "Renderer restarting...");
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

    if (state->mode != RUNNING)
    {
        // SetTargetFPS(DEFAULT_REFRESH_RATE);
    }

    state->frameTime = GetFrameTime();
    state->fps = GetFPS();
}

void UpdateViewport(AppState *state)
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
    SetConfigFlags(DEFAULT_CONFIG_FLAGS);
    InitWindow(state.windowSize.x, state.windowSize.y, TextFormat("%s v%s | %s", toolName, toolVersion, toolDescription));
    SearchAndSetResourceDir(DEFAULT_RESOURCE_FOLDER);
    GuiLoadStyle(DEFAULT_GUI_STYLE);

    CreateRenderPipeline(&state);
    /* -------------------------------- Main Loop ------------------------------- */
    while (!WindowShouldClose())
    {
        UpdateInput(&state);
        UpdateRenderState(&state);
        UpdateViewport(&state);
        UpdateMouseCursorIcon(&state);
    }

    DeleteRenderPipeline(&state);
    CloseWindow();

    return 0;
}
