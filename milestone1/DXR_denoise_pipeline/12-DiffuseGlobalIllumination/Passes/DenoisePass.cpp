
#include "DenoisePass.h"

namespace {
    // Where is our shaders located?  -- we will directly find in the Data folder
    const char* kDenoiseFragShader = "Tutorial12\\bmfrDenoise.ps.hlsl";
    //const char* kDenoiseFragShader = "Tutorial12\\simpleDiffuseGI.rt.hlsl";
};

BlockwiseMultiOrderFeatureRegression::BlockwiseMultiOrderFeatureRegression(const std::string &bufferToDenoise)
    : ::RenderPass("BMFR Denoise Pass", "BMFR Denoise Options")
{
    mDenoiseChannel = bufferToDenoise;
}

bool BlockwiseMultiOrderFeatureRegression::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
    if (!pResManager) return false;

    // Stash our resource manager; ask for the texture the developer asked us to accumulate
    mpResManager = pResManager;
    mpResManager->requestTextureResource(mDenoiseChannel); //current frame image
    mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse" }); //three feature buffers

    // Create our graphics state and accumulation shader
    mpGfxState = GraphicsState::create();
    mpDenoiseShader = FullscreenLaunch::create(kDenoiseFragShader);

    // Our GUI needs less space than other passes, so shrink the GUI window.
    setGuiSize(ivec2(250, 135));

    return true;
}

void BlockwiseMultiOrderFeatureRegression::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{

    // When our renderer moves around, we want to reset accumulation
    mpScene = pScene;
}

void BlockwiseMultiOrderFeatureRegression::resize(uint32_t width, uint32_t height)
{
    // We need a framebuffer to attach to our graphics pipe state (when running our full-screen pass)
    mpInternalFbo = ResourceManager::createFbo(width, height, ResourceFormat::RGBA32Float);
    mpGfxState->setFbo(mpInternalFbo);
}

void BlockwiseMultiOrderFeatureRegression::renderGui(Gui* pGui)
{
    int dirty = 0;
    dirty |= (int)pGui->addCheckBox(mDoDenoise ? "Do BMFR Denoise" : "Ignore the denoise stage", mDoDenoise);

    if (dirty) setRefreshFlag();
}


void BlockwiseMultiOrderFeatureRegression::execute(RenderContext* pRenderContext)
{
    // Grab the texture to accumulate
    Texture::SharedPtr inputTexture = mpResManager->getTexture(mDenoiseChannel);

    // If our input texture is invalid, or we've been asked to skip accumulation, do nothing.
    if (!inputTexture || !mDoDenoise) return;

    // Set shader parameters for our accumulation
    auto denoiseShaderVars = mpDenoiseShader->getVars();
    //pass four variables in, world pos, world normal, diffuse color and curr frame image
    denoiseShaderVars["gPos"] = mpResManager->getTexture("WorldPosition");
    denoiseShaderVars["gNorm"] = mpResManager->getTexture("WorldNormal");
    denoiseShaderVars["gDiffuseMatl"] = mpResManager->getTexture("MaterialDiffuse");
    denoiseShaderVars["gCurFrame"] = inputTexture;

    // Do the accumulation
    mpDenoiseShader->execute(pRenderContext, mpGfxState);

    // We've accumulated our result.  Copy that back to the input/output buffer
    pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), inputTexture->getRTV());
}