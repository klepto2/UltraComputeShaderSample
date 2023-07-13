#include "UltraEngine.h"
#include "Components/Mover.hpp"
#include "Compute/ComputeShader.h"

using namespace UltraEngine;
using namespace UltraEngine::Compute;


/// <summary>
/// Sample Buffer structure
/// The Padding is needed because in vulkan the buffer passed to a shader needs to have an alignment of 16.
/// </summary>
struct SampleComputeParameters
{
    float size;
    float padding1;
    float padding2;
    float padding3;
    Vec4 color;
};

int main(int argc, const char* argv[])
{
    //Get the displays
    auto displays = GetDisplays();

    //Create a window
    auto window = CreateWindow("Ultra Engine", 0, 0, 1280, 720, displays[0], WINDOW_CENTER | WINDOW_TITLEBAR);

    //Create a world
    auto world = CreateWorld();

    //Create a framebuffer
    auto framebuffer = CreateFramebuffer(window);

    //Create a camera
    auto camera = CreateCamera(world);
    camera->SetClearColor(0.125);
    camera->SetFov(70);
    camera->SetPosition(0, 0, -3);

    //Create a light
    auto light = CreateBoxLight(world);
    light->SetRotation(35, 45, 0);
    light->SetRange(-10, 10);


    SampleComputeParameters sampleParameters;
    sampleParameters.size = 64.0;
    sampleParameters.color = Vec4(1.0, 0.0, 0.0, 1.0);
   
    // Create a first computeshader for uniform buffer usage
    // Normally you will use uniform buffers for data which is not changing, this is just for showcase 
    // and shows that the data can still be updated at runtime
    auto sampleComputePipeLine_Unifom = ComputeShader::Create("Shaders/Compute/simple_test.comp.spv");
    auto targetTexture_uniform = CreateTexture(TEXTURE_2D, 512, 512, TEXTURE_RGBA32, {}, 1, TEXTURE_STORAGE, TEXTUREFILTER_LINEAR);
    // Now we define the descriptor layout, the binding is resolved by the order in which the items are added
    sampleComputePipeLine_Unifom->AddTargetImage(targetTexture_uniform); // Seting up a target image --> layout 0
    sampleComputePipeLine_Unifom->AddUniformBuffer(&sampleParameters, sizeof(SampleComputeParameters), false); // Seting up a uniform bufffer --> layout 1

    // Create a first computeshader for push constant usage
    // This is the better way to pass dynamic data
    auto sampleComputePipeLine_Push = ComputeShader::Create("Shaders/Compute/simple_test_push.comp.spv");
    auto targetTexture_push= CreateTexture(TEXTURE_2D, 512, 512, TEXTURE_RGBA32, {}, 1, TEXTURE_STORAGE, TEXTUREFILTER_LINEAR);
    sampleComputePipeLine_Push->AddTargetImage(targetTexture_push);
    sampleComputePipeLine_Push->SetupPushConstant(sizeof(SampleComputeParameters)); // Currently used to initalize the pipeline, may change in the future

    // For demonstration the push based shader is executed continously
    // The push-constant data is passed here
    sampleComputePipeLine_Push->BeginDispatch(world, targetTexture_uniform->GetSize().x / 16.0, targetTexture_uniform->GetSize().y / 16.0, 1, false, ComputeHook::TRANSFER, &sampleParameters, sizeof(SampleComputeParameters));


    //Create a box
    auto box_uniform = CreateBox(world);
    box_uniform->Move(-1.0, 0.0, 0.0);
    
    auto box_uniform_mtl = CreateMaterial();
    box_uniform_mtl->SetShaderFamily(LoadShaderFamily("Shaders/pbr.json"));
    box_uniform_mtl->SetTexture(targetTexture_uniform);
    box_uniform->SetMaterial(box_uniform_mtl);

    //Create a second box
    auto box_push = CreateBox(world);
    box_push->Move(1.0, 0.0, 0.0);

    auto box_push_mtl = CreateMaterial();
    box_push_mtl->SetShaderFamily(LoadShaderFamily("Shaders/pbr.json"));
    box_push_mtl->SetTexture(targetTexture_push);
    box_push->SetMaterial(box_push_mtl);

    //Entity component system
    auto component_uniform = box_uniform->AddComponent<Mover>();
    component_uniform->rotationspeed.y = 45;

    auto component_push = box_push->AddComponent<Mover>();
    component_push->rotationspeed.y = 45;

    auto camera2D = CreateCamera(world, UltraEngine::PROJECTION_ORTHOGRAPHIC);
    camera2D->SetDepthPrepass(false);
    camera2D->SetClearMode(UltraEngine::CLEAR_DEPTH);
    camera2D->SetRenderLayers(1);
    camera2D->SetPosition((float)framebuffer->size.x * 0.5f, (float)framebuffer->size.y * 0.5f, 0);

    //Create sprite

    const int fontsize = 14;
    const string text = "Compute Shader: sample\n" 
                        "Left : Unifrom buffer usage, updated when keys (1,2,3) are pressed\n"
                        "Right: Push constant usage, updated continously\n";
    auto font = LoadFont("Fonts/arial.ttf");
    auto sprite = CreateSprite(world, font, 
       text, fontsize);
    sprite->SetRenderLayers(1);
    sprite->SetPosition(20, window->GetSize().height - 45);

    world->RecordStats(true);

    //Main loop
    while (window->Closed() == false and window->KeyDown(KEY_ESCAPE) == false)
    {
        if (window->KeyHit(KEY_D1))
        {
            // Modify the shader data
            sampleParameters.size = 64.0;
            sampleParameters.color = Vec4(1.0, 0.0, 0.0, 1.0);
            sampleComputePipeLine_Unifom->Update(1); // Notify the uniform shader to update the buffer at layout binding 1

            // Queue the dispatch to the cmd-pipeline just once.
            sampleComputePipeLine_Unifom->BeginDispatch(world, targetTexture_uniform->GetSize().x / 16.0, targetTexture_uniform->GetSize().y / 16.0, 1, true, ComputeHook::TRANSFER);
        }
        if (window->KeyHit(KEY_D2))
        {
            sampleParameters.size = 128.0;
            sampleParameters.color = Vec4(1.0, 1.0, 0.0, 1.0);
            sampleComputePipeLine_Unifom->Update(1);
            sampleComputePipeLine_Unifom->BeginDispatch(world, targetTexture_uniform->GetSize().x / 16.0, targetTexture_uniform->GetSize().y / 16.0, 1, true, ComputeHook::TRANSFER);
        }
        if (window->KeyHit(KEY_D3))
        {
            sampleParameters.size = 16.0;
            sampleParameters.color = Vec4(0.0, 0.0, 1.0, 1.0);
            sampleComputePipeLine_Unifom->Update(1);
            sampleComputePipeLine_Unifom->BeginDispatch(world, targetTexture_uniform->GetSize().x / 16.0, targetTexture_uniform->GetSize().y / 16.0, 1, true, ComputeHook::TRANSFER);
        }


        world->Update();
        world->Render(framebuffer);

        // Collect the compute timings (returns the start and and times when the shaders are executed and finished)
        auto result_uniform = sampleComputePipeLine_Unifom->GetQueryTimer()->GetQueryPoolResults();
        auto result_push = sampleComputePipeLine_Push->GetQueryTimer()->GetQueryPoolResults();

        window->SetText("ComputeSample: FPS - " + String(world->renderstats.framerate) 
            + " ComputeTime_Uniform (ms) - " + String((result_uniform[1] - result_uniform[0]) / 1e+6f)
            + " ComputeTime_Push (ms) - " + String((result_push[1] - result_push[0]) / 1e+6f));

    }
    return 0;
}