//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include "Globals.h"

GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
    GLchar  infoLogBuffer[1024] = {};
    GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
    GLsizei infoLogSize;
    GLint   success;

    char versionString[] = "#version 430\n";
    char shaderNameDefine[128];
    sprintf(shaderNameDefine, "#define %s\n", shaderName);
    char vertexShaderDefine[] = "#define VERTEX\n";
    char fragmentShaderDefine[] = "#define FRAGMENT\n";

    const GLchar* vertexShaderSource[] = {
        versionString,
        shaderNameDefine,
        vertexShaderDefine,
        programSource.str
    };
    const GLint vertexShaderLengths[] = {
        (GLint)strlen(versionString),
        (GLint)strlen(shaderNameDefine),
        (GLint)strlen(vertexShaderDefine),
        (GLint)programSource.len
    };
    const GLchar* fragmentShaderSource[] = {
        versionString,
        shaderNameDefine,
        fragmentShaderDefine,
        programSource.str
    };
    const GLint fragmentShaderLengths[] = {
        (GLint)strlen(versionString),
        (GLint)strlen(shaderNameDefine),
        (GLint)strlen(fragmentShaderDefine),
        (GLint)programSource.len
    };

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint programHandle = glCreateProgram();
    glAttachShader(programHandle, vshader);
    glAttachShader(programHandle, fshader);
    glLinkProgram(programHandle);
    glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    glUseProgram(0);

    glDetachShader(programHandle, vshader);
    glDetachShader(programHandle, fshader);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
    String programSource = ReadTextFile(filepath);

    Program program = {};
    program.handle = CreateProgramFromSource(programSource, programName);
    program.filepath = filepath;
    program.programName = programName;
    program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);

    GLint attributeCount = 0;
    glGetProgramiv(program.handle, GL_ACTIVE_ATTRIBUTES, &attributeCount);

    for (GLuint i = 0; i < attributeCount; i++)
    {
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        GLchar name[256];
        glGetActiveAttrib(program.handle, i,
            ARRAY_COUNT(name),
            &length,
            &size,
            &type,
            name);

        u8 location = glGetAttribLocation(program.handle, name);
        program.shaderLayout.attributes.push_back(VertexShaderAttribute{ location, (u8)size });
    }

    app->programs.push_back(program);

    return app->programs.size() - 1;
}

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program)
{
    GLuint ReturnValue = 0;

    SubMesh& Submesh = mesh.submeshes[submeshIndex];
    for (u32 i = 0; i < (u32)Submesh.vaos.size(); ++i)
    {
        if (Submesh.vaos[i].programHandle == program.handle)
        {
            ReturnValue = Submesh.vaos[i].handle;
            break;
        }
    }

    if (ReturnValue == 0)
    {
        glGenVertexArrays(1, &ReturnValue);
        glBindVertexArray(ReturnValue);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

        auto& ShaderLayout = program.shaderLayout.attributes;
        for (auto ShaderIt = ShaderLayout.cbegin(); ShaderIt != ShaderLayout.cend(); ++ShaderIt)
        {
            bool attributeWasLinked = false;
            auto SubmeshLayout = Submesh.vertexBufferLayout.attributes;
            for (auto SubmeshIt = SubmeshLayout.cbegin(); SubmeshIt != SubmeshLayout.cend(); ++SubmeshIt)
            {
                if (ShaderIt->location == SubmeshIt->location)
                {
                    const u32 index = SubmeshIt->location;
                    const u32 ncomp = SubmeshIt->componentCount;
                    const u32 offset = SubmeshIt->offset + Submesh.vertexOffset;
                    const u32 stride = Submesh.vertexBufferLayout.stride;

                    glVertexAttribPointer(index, ncomp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)(offset));
                    glEnableVertexAttribArray(index);

                    attributeWasLinked = true;
                    break;
                }
            }
            assert(attributeWasLinked);
        }
        glBindVertexArray(0);

        VAO vao = { ReturnValue, program.handle };
        Submesh.vaos.push_back(vao);
    }

    return ReturnValue;
}

glm::mat4 TransformScale(const vec3& scaleFactors)
{
    return glm::scale(scaleFactors);
}

glm::mat4 TransformPositionScale(const vec3& position, const vec3& scaleFactors)
{
    glm::mat4 ReturnValue = glm::translate(position);
    ReturnValue = glm::scale(ReturnValue, scaleFactors);
    return ReturnValue;
}

void Init(App* app)
{
    // TODO: Initialize your resources here!
    // - vertex buffers
    // - element/index buffers
    // - vaos
    // - programs (and retrieve uniform indices)
    // - textures

    //Get OPENGL info.
    app->openglDebugInfo += "OpeGL version:\n" + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    glGenBuffers(1, &app->embeddedVertices);
    glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &app->embeddedElements);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &app->vao);
    glBindVertexArray(app->vao);
    glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    app->LoadWaterVAO();

    app->renderToBackBufferShader = LoadProgram(app, "RENDER_TO_BB.glsl", "RENDER_TO_BB");
    app->renderToFrameBufferShader = LoadProgram(app, "RENDER_TO_FB.glsl", "RENDER_TO_FB");
    app->framebufferToQuadShader = LoadProgram(app, "FB_TO_BB.glsl", "FB_TO_BB");
    app->waterShader = LoadProgram(app, "WATER_SHADER.glsl", "WATER_SHADER");

    const Program& texturedMeshProgram = app->programs[app->renderToFrameBufferShader];
    app->texturedMeshProgram_uTexture = glGetUniformLocation(texturedMeshProgram.handle, "uTexture");

    u32 PatrickModelIndex = ModelLoader::LoadModel(app, "Patrick/Patrick.obj");
    u32 GroundModelIndex = ModelLoader::LoadModel(app, "Patrick/Ground.obj");
    u32 ShrekModelIndex = ModelLoader::LoadModel(app, "Patrick/Shrek.obj");
    u32 LuffyModelIndex = ModelLoader::LoadModel(app, "Patrick/Luffy.obj");

    app->CubeModelIndex = ModelLoader::LoadModel(app, "Patrick/cube.obj");
    app->SphereModelIndex = ModelLoader::LoadModel(app, "Patrick/sphere.obj");


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAlignment);

    app->localUniformBuffer = CreateConstantBuffer(app->maxUniformBufferSize);

    app->entities.push_back({ TransformPositionScale(vec3(0.0, 0.0, -4.0), vec3(1.0, 1.0, 1.0)),PatrickModelIndex,0,0 });
    app->entities.push_back({ TransformPositionScale(vec3(-4.0, 0.0, -5.0), vec3(1.0, 1.0, 1.0)),PatrickModelIndex,0,0 });
    app->entities.push_back({ TransformPositionScale(vec3(4.0, 0.0, -3.0), vec3(1.0, 1.0, 1.0)),PatrickModelIndex,0,0 });
    
    //app->entities.push_back({ TransformPositionScale(vec3(0.0, -4.0, 0.0), vec3(1.0, 1.0, 1.0)), GroundModelIndex, 0, 0 });

    app->entities.push_back({ TransformPositionScale(vec3(-5.0, -4.0, 5.0), vec3(2.0, 2.0, 2.0)), ShrekModelIndex, 0, 0 });
    app->entities.push_back({ TransformPositionScale(vec3(6.0, -4.0, 5.0), vec3(0.03, 0.03, 0.03)), LuffyModelIndex, 0, 0 });

    app->WaterWorldMatrix = TransformPositionScale(vec3(0.0, -2.0, 0.0), vec3(20.0, 20.0, 20.0));
    app->WaterWorldMatrix = glm::rotate(app->WaterWorldMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    app->CreateDirectLight(vec3(1.0, 1.0, 1.0), vec3(1.0, -1.0, 1.0), vec3(5.0, -3.0, 0.0));
    app->CreateDirectLight(vec3(1.0, 1.0, 1.0), vec3(-1.0, -1.0, -1.0), vec3(-5.0, -3.0, 0.0));

    app->CreatePointLight(vec3(1.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, -3.0));
    app->CreatePointLight(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 1.0), vec3(-4.0, -3.0, 6.0));
    app->CreatePointLight(vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 1.0), vec3(4.0, -3.0, 6.0));

    //
    app->ConfigureWaterBuffer(app->waterBuffers.fboReflection, app->waterBuffers.rtReflection, app->waterBuffers.rtReflectionDepth);
    app->ConfigureWaterBuffer(app->waterBuffers.fboRefraction, app->waterBuffers.rtRefraction, app->waterBuffers.rtRefractionDepth);
    app->ConfigureFrameBuffer(app->deferredFrameBuffer);

    app->mode = Mode_Deferred;
    
}

void App::CreateDirectLight(vec3 color, vec3 direction, vec3 position) 
{
    lights.push_back({ LightType::LightType_Directional, color, direction, position });

    entities.push_back({ TransformPositionScale(position, vec3(0.5)),CubeModelIndex,0,0 });
}

void App::CreatePointLight(vec3 color, vec3 direction, vec3 position) 
{
    lights.push_back({ LightType::LightType_Point, color, direction, position });

    entities.push_back({ TransformPositionScale(position, vec3(0.5)),SphereModelIndex,0,0 });
}

void Gui(App* app)
{
    ImGui::Begin("Info");
    ImGui::Text("FPS: %f", 1.0f / app->deltaTime);
    ImGui::Text("%s", app->openglDebugInfo.c_str());

    const char* RenderModes[] = { "FORWARD", "DEFERRED" };
    if (ImGui::BeginCombo("Render Mode", RenderModes[app->mode]))
    {
        for (size_t i = 0; i < ARRAY_COUNT(RenderModes); i++)
        {
            bool isSelected = (i == app->mode);
            if (ImGui::Selectable(RenderModes[i], isSelected))
            {
                app->mode = static_cast<Mode>(i);
            }
        }
        ImGui::EndCombo();
    }

    if (app->mode == Mode::Mode_Deferred)
    {
        for (size_t i = 0; i < app->deferredFrameBuffer.colorAttachment.size(); i++)
        {
            ImGui::Image((ImTextureID)app->deferredFrameBuffer.colorAttachment[i], ImVec2(250, 150), ImVec2(0, 1), ImVec2(1, 0));
        }
        ImGui::Text("Water Reflection FrameBuffer");
        
        if (app->waterBuffers.GetReflectionTexture() != 0)
        {
            ImGui::Image((ImTextureID)app->waterBuffers.GetReflectionTexture(), ImVec2(250, 150), ImVec2(0, 1), ImVec2(1, 0));
        }
        else
        {
            ELOG("WATER REFLECTION TEXTURE NOT LOADED");
        }

        ImGui::Text("Water Refraction FrameBuffer");
        if (app->waterBuffers.GetRefractionTexture() != 0)
        {
            ImGui::Image((ImTextureID)app->waterBuffers.GetRefractionTexture(), ImVec2(250, 150), ImVec2(0, 1), ImVec2(1, 0));
        }
        else
        {
            ELOG("WATER REFRACTION TEXTURE NOT LOADED");
        }
    }    
    ImGui::End();
}

void Update(App* app)
{
    // You can handle app->input keyboard/mouse here
}



void Render(App* app)
{
    switch (app->mode)
    {
    case Mode_Forward:
    {

        app->UpdateEntityBuffer(true);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glViewport(0, 0, app->displaySize.x, app->displaySize.y);

        const Program& ForwardProgram = app->programs[app->renderToBackBufferShader];
        glUseProgram(ForwardProgram.handle);

        app->RenderGeometry(ForwardProgram, vec4(0, -1, 0, 15));


    }
    break;
    case Mode_Deferred:
    {

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, app->displaySize.x, app->displaySize.y);

        /////////////////////////////////////////////////////////////////////////////////////////// Water Reflection FBO

        glEnable(GL_CLIP_DISTANCE0);

        glBindFramebuffer(GL_FRAMEBUFFER, app->waterBuffers.fboReflection.fbHandle);
        GLuint reflectionBuffers[] = { app->waterBuffers.fboReflection.fbHandle };
        glDrawBuffers(app->waterBuffers.fboReflection.colorAttachment.size(), reflectionBuffers);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //Move camera down water before rendering
        float distance = 2 * (app->sceneCam.cameraPos.y - app->GetHeight(app->WaterWorldMatrix));
        app->sceneCam.cameraPos.y -= distance;
        app->sceneCam.pitch = -app->sceneCam.pitch;

        const Program& DeferredProgram = app->programs[app->renderToFrameBufferShader];
        glUseProgram(DeferredProgram.handle);
        app->UpdateEntityBuffer(false);
        app->RenderGeometry(DeferredProgram, vec4(0, 1, 0, -app->GetHeight(app->WaterWorldMatrix)));

        //Return camero to original pos
        app->sceneCam.cameraPos.y += distance;
        app->sceneCam.pitch = -app->sceneCam.pitch;

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        /////////////////////////////////////////////////////////////////////////////////////////// Water Refraction FBO

        glBindFramebuffer(GL_FRAMEBUFFER, app->waterBuffers.fboRefraction.fbHandle);
        GLuint refractionBuffers[] = { app->waterBuffers.fboRefraction.fbHandle };
        glDrawBuffers(app->waterBuffers.fboRefraction.colorAttachment.size(), refractionBuffers);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(DeferredProgram.handle);
        app->UpdateEntityBuffer(false);
        app->RenderGeometry(DeferredProgram, vec4(0, -1, 0, app->GetHeight(app->WaterWorldMatrix)));

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        /////////////////////////////////////////////////////////////////////////////////////////// Deferred FBO

        glDisable(GL_CLIP_DISTANCE0);
        glBindFramebuffer(GL_FRAMEBUFFER, app->deferredFrameBuffer.fbHandle);

        GLuint drawBuffers[] = { app->deferredFrameBuffer.fbHandle };
        glDrawBuffers(app->deferredFrameBuffer.colorAttachment.size(), drawBuffers);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(DeferredProgram.handle);
        app->UpdateEntityBuffer(true);
        app->RenderGeometry(DeferredProgram, vec4(0, -1, 0, 3));

        glUseProgram(0);

        const Program& FwClipp = app->programs[app->waterShader];
        glUseProgram(FwClipp.handle);
        app->RenderWater(FwClipp);

        glUseProgram(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ///////////////////////////////////////////////////////////////////////////////////////////

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, app->displaySize.x, app->displaySize.y);

        const Program& FBToBB = app->programs[app->framebufferToQuadShader];
        glUseProgram(FBToBB.handle);

        glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->localUniformBuffer.handle, app->globalParamsOffset, app->globalParamsSize);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[0]);
        glUniform1i(glGetUniformLocation(FBToBB.handle, "uAlbedo"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[1]);
        glUniform1i(glGetUniformLocation(FBToBB.handle, "uNormals"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[2]);
        glUniform1i(glGetUniformLocation(FBToBB.handle, "uPosition"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[3]);
        glUniform1i(glGetUniformLocation(FBToBB.handle, "uViewDir"), 3);

        glBindVertexArray(app->vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glBindVertexArray(0);
        glUseProgram(0);
    }
    break;

    default:;
    }
}

void App::UpdateEntityBuffer(bool mouse)
{

    float aspectRatio = (float)displaySize.x / (float)displaySize.y;
    float znear = 0.1f;
    float zfar = 1000.0f;
    projection = glm::perspective(glm::radians(60.0f), aspectRatio, znear, zfar);

    if(mouse) processInput(glfwGetCurrentContext());

    view = glm::lookAt(sceneCam.cameraPos, sceneCam.cameraPos + sceneCam.cameraFront, sceneCam.cameraUp);

    u32 cont = 0;

    BufferManager::MapBuffer(localUniformBuffer, GL_WRITE_ONLY);

    //Push lights global params
    globalParamsOffset = localUniformBuffer.head;
    PushVec3(localUniformBuffer, sceneCam.cameraPos);
    PushUInt(localUniformBuffer, lights.size());

    for (size_t i = 0; i < lights.size(); ++i)
    {
        BufferManager::AlignHead(localUniformBuffer, sizeof(vec4));

        Light& light = lights[i];
        PushUInt(localUniformBuffer, light.type);
        PushVec3(localUniformBuffer, light.color);
        //light.direction.y = sin(deltaTime + 100.0) * 1000.0;
        PushVec3(localUniformBuffer, light.direction);
        PushVec3(localUniformBuffer, light.position);
    }
    globalParamsSize = localUniformBuffer.head - globalParamsOffset;


    for (auto it = entities.begin(); it != entities.end(); ++it)
    {

        glm::mat4 world = it->worldMatrix;
        glm::mat4 WVP = projection * view * world;

        Buffer& localBuffer = localUniformBuffer;
        BufferManager::AlignHead(localBuffer, uniformBlockAlignment);
        it->localParamsOffset = localBuffer.head;
        PushMat4(localBuffer, world);
        PushMat4(localBuffer, WVP);
        it->localParamsSize = localBuffer.head - it->localParamsOffset;
        ++cont;
    }

    BufferManager::UnmapBuffer(localUniformBuffer);
}

void App::RenderWater(const Program& aBindedProgram)
{   
    
    // Obt�n las ubicaciones de las variables uniformes en el shader
    GLuint viewLoc = glGetUniformLocation(aBindedProgram.handle, "viewMatrix");
    GLuint projLoc = glGetUniformLocation(aBindedProgram.handle, "projectionMatrix");
    GLuint modelLoc = glGetUniformLocation(aBindedProgram.handle, "modelMatrix");

    // Env�a las matrices al shader
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &WaterWorldMatrix[0][0]);

    GLuint reflectTexLoc = glGetUniformLocation(aBindedProgram.handle, "reflectionTexture");
    GLuint refractTexLoc = glGetUniformLocation(aBindedProgram.handle, "refractionTexture");

    //Attach Textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, waterBuffers.GetReflectionTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, waterBuffers.GetRefractionTexture());

    glUniform1i(reflectTexLoc, 0);
    glUniform1i(refractTexLoc, 1);

    // Enlaza el VAO del agua
    glBindVertexArray(waterVAO);

    // Dibuja los elementos (tri�ngulos)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Desenlaza el VAO
    glBindVertexArray(0);
}

void App::ConfigureFrameBuffer(FrameBuffer& aConfigFB)
{
    aConfigFB.Clear();

    aConfigFB.colorAttachment.push_back(CreateTexture());
    aConfigFB.colorAttachment.push_back(CreateTexture(true));
    aConfigFB.colorAttachment.push_back(CreateTexture(true));
    aConfigFB.colorAttachment.push_back(CreateTexture(true));

    glGenTextures(1, &aConfigFB.depthHandle);
    glBindTexture(GL_TEXTURE_2D, aConfigFB.depthHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, displaySize.x, displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &aConfigFB.fbHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, aConfigFB.fbHandle);

    std::vector<GLuint> drawBuffers;
    for (size_t i = 0; i < aConfigFB.colorAttachment.size(); i++)
    {
        GLuint position = GL_COLOR_ATTACHMENT0 + i;
        glFramebufferTexture(GL_FRAMEBUFFER, position, aConfigFB.colorAttachment[i], 0);
        drawBuffers.push_back(position);
    }

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, aConfigFB.depthHandle, 0);

    glDrawBuffers(drawBuffers.size(), drawBuffers.data());

    GLenum frameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(frameBufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        int i = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::ConfigureWaterBuffer(FrameBuffer& aConfigFB, GLuint& colorAttach, GLuint& depth)
{
    aConfigFB.Clear();

    colorAttach = CreateTexture();

    aConfigFB.colorAttachment.push_back(colorAttach);

    glGenTextures(1, &aConfigFB.depthHandle);
    glBindTexture(GL_TEXTURE_2D, aConfigFB.depthHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, displaySize.x, displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    depth = aConfigFB.depthHandle;

    glGenFramebuffers(1, &aConfigFB.fbHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, aConfigFB.fbHandle);

    std::vector<GLuint> drawBuffers;
    for (size_t i = 0; i < aConfigFB.colorAttachment.size(); i++)
    {
        GLuint position = GL_COLOR_ATTACHMENT0 + i;
        glFramebufferTexture(GL_FRAMEBUFFER, position, aConfigFB.colorAttachment[i], 0);
        drawBuffers.push_back(position);
    }

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, aConfigFB.depthHandle, 0);

    glDrawBuffers(drawBuffers.size(), drawBuffers.data());

    GLenum frameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (frameBufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        int i = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::RenderGeometry(const Program& aBindedProgram, vec4 clippingPlane)
{
    glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), localUniformBuffer.handle, globalParamsOffset, globalParamsSize);

    GLuint planeLoc = glGetUniformLocation(aBindedProgram.handle, "plane");
    glUniform4f(planeLoc, clippingPlane.x, clippingPlane.y, clippingPlane.z, clippingPlane.w);

    for (auto it = entities.begin(); it != entities.end(); ++it)
    {

        glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), localUniformBuffer.handle, it->localParamsOffset, it->localParamsSize);

        Model& model = models[it->modelIndex];
        Mesh& mesh = meshes[model.meshIdx];

        //glUniformMatrix4fv(glGetUniformLocation(texturedMeshProgram.handle, "WVP"), 1, GL_FALSE, &WVP[0][0]);

        for (u32 i = 0; i < mesh.submeshes.size(); ++i)
        {
            GLuint vao = FindVAO(mesh, i, aBindedProgram);
            glBindVertexArray(vao);

            u32 subMeshmaterialIdx = model.materialIdx[i];
            Material& subMeshMaterial = materials[subMeshmaterialIdx];

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[subMeshMaterial.albedoTextureIdx].handle);
            glUniform1i(texturedMeshProgram_uTexture, 0);

            SubMesh& submesh = mesh.submeshes[i];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
        }

    }
}

const GLuint App::CreateTexture(const bool isFloatingPoint)
{
    GLuint textureHandle;

    GLenum internalFormat = isFloatingPoint ? GL_RGBA16F : GL_RGBA8;
    GLenum format = GL_RGBA;
    GLenum dataType = isFloatingPoint ? GL_FLOAT : GL_UNSIGNED_BYTE;

    glGenTextures(1, &textureHandle);
    glBindTexture(GL_TEXTURE_2D, textureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, displaySize.x, displaySize.y, 0, format, dataType, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureHandle;
}

void App::processInput(GLFWwindow* window)
{
    float cameraSpeed = 5.f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        sceneCam.cameraPos += cameraSpeed * sceneCam.cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        sceneCam.cameraPos -= cameraSpeed * sceneCam.cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        sceneCam.cameraPos -= glm::normalize(glm::cross(sceneCam.cameraFront, sceneCam.cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        sceneCam.cameraPos += glm::normalize(glm::cross(sceneCam.cameraFront, sceneCam.cameraUp)) * cameraSpeed;
}

void App::LoadWaterVAO()
{
    // Suponiendo que tienes los datos de los v�rtices y los �ndices definidos
    float waterVertices[] = {
        // posiciones de los v�rtices
        -0.5f, 0.0f, -0.5f, 0.0f, 0.0f, //0
         0.5f, 0.0f, -0.5f, 1.0f, 0.0f, //1
         0.5f, 0.0f,  0.5f, 1.0f, 1.0f, //2
        -0.5f, 0.0f,  0.5f, 0.0f, 1.0f  //3
    };

    unsigned int waterIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glGenBuffers(1, &waterEBO);

    glBindVertexArray(waterVAO);

    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(waterVertices), waterVertices, GL_STATIC_DRAW);

    //vertex position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //vertex position
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    //indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(waterIndices), waterIndices, GL_STATIC_DRAW);

    glBindVertexArray(0);
}

float App::GetHeight(glm::mat4 transformMat)
{
    vec3 pos = transformMat[3]; // 4th column of the model matrix

    float D = pos.y;

    return D;
}


