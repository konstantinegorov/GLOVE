/**
 * Copyright (C) 2015-2018 Think Silicon S.A. (https://think-silicon.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public v3
 * License as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

/**
 *  @file       shaderProgram.cpp
 *  @author     Think Silicon
 *  @date       25/07/2018
 *  @version    1.0
 *
 *  @brief      Shader Program Functionality in GLOVE
 *
 *  @scope
 *
 *  A Shader Program represents fully processed executable code, in the OpenGL ES
 *  Shading Language (ESSL) for one or more Shader stages.
 *
 */

#include "shaderProgram.h"
#include "context/context.h"

ShaderProgram::ShaderProgram(const vulkanAPI::vkContext_t *vkContext)
: refObject()
{
    FUN_ENTRY(GL_LOG_TRACE);

    mVkContext = vkContext;

    mShaders[0] = nullptr;
    mShaders[1] = nullptr;

    mStagesIDs[0] = -1;
    mStagesIDs[1] = -1;

    mMinDepthRange = 1.f;
    mMaxDepthRange = 0.f;

    mVkShaderModules[0] = VK_NULL_HANDLE;
    mVkShaderModules[1] = VK_NULL_HANDLE;

    mVkDescSetLayout = VK_NULL_HANDLE;
    mVkDescSetLayoutBind = nullptr;
    mVkDescPool = VK_NULL_HANDLE;
    mVkDescSet = VK_NULL_HANDLE;
    mVkPipelineLayout = VK_NULL_HANDLE;

    mPipelineCache = new vulkanAPI::PipelineCache(mVkContext);

    mStageCount = 0;

    mUpdateDescriptorSets = false;
    mUpdateDescriptorData = false;
    mLinked = false;
    mIsPrecompiled = false;
    mValidated = false;
    mActiveVertexVkBuffersCount = 0;
    mActiveIndexVkBuffer = VK_NULL_HANDLE;
    mExplicitIbo = nullptr;

    SetPipelineVertexInputStateInfo();
}

ShaderProgram::~ShaderProgram()
{
    FUN_ENTRY(GL_LOG_TRACE);

    ReleaseVkObjects();

    if(mPipelineCache) {
        delete mPipelineCache;
        mPipelineCache = nullptr;
    }

    if(mExplicitIbo != nullptr) {
        delete mExplicitIbo;
        mExplicitIbo = nullptr;
    }
}

bool
ShaderProgram::SetPipelineShaderStage(uint32_t &pipelineShaderStageCount, int *pipelineShaderStagesIDs, VkPipelineShaderStageCreateInfo *pipelineShaderStages)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    bool linked = true;

    pipelineShaderStageCount = GetStageCount();
    if(pipelineShaderStageCount == 1) {
        pipelineShaderStages[0].flags  = 0;
        pipelineShaderStages[0].pNext  = nullptr;
        pipelineShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStages[0].stage  = GetShaderStage();
        pipelineShaderStages[0].module = GetShaderModule();
        pipelineShaderStages[0].pName  = "main\0";
        pipelineShaderStages[0].pSpecializationInfo = nullptr;
        pipelineShaderStagesIDs[0]     = GetStagesIDs(0);

        if(GetShaderModule() == VK_NULL_HANDLE) {
            linked = false;
        }
    } else if (pipelineShaderStageCount == 2) {
        pipelineShaderStages[0].flags  = 0;
        pipelineShaderStages[0].pNext  = nullptr;
        pipelineShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        pipelineShaderStages[0].module = GetVertexShaderModule();
        pipelineShaderStages[0].pName  = "main\0";
        pipelineShaderStages[0].pSpecializationInfo = nullptr;
        pipelineShaderStagesIDs[0]     = GetStagesIDs(0);

        if(GetVertexShaderModule() == VK_NULL_HANDLE) {
            linked = false;
        }

        pipelineShaderStages[1].flags  = 0;
        pipelineShaderStages[1].pNext  = nullptr;
        pipelineShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipelineShaderStages[1].module = GetFragmentShaderModule();
        pipelineShaderStages[1].pName  = "main\0";
        pipelineShaderStages[1].pSpecializationInfo = nullptr;
        pipelineShaderStagesIDs[1]     = GetStagesIDs(1);

        if(GetFragmentShaderModule() == VK_NULL_HANDLE) {
            linked = false;
        }
    } else {
        linked = false;
    }

    return linked;
}

void
ShaderProgram::SetPipelineVertexInputStateInfo(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mVkPipelineVertexInput.sType                            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    mVkPipelineVertexInput.pNext                            = nullptr;
    mVkPipelineVertexInput.flags                            = 0;
    mVkPipelineVertexInput.vertexBindingDescriptionCount    = 0;
    mVkPipelineVertexInput.pVertexBindingDescriptions       = mVkVertexInputBinding;
    mVkPipelineVertexInput.vertexAttributeDescriptionCount  = 0;
    mVkPipelineVertexInput.pVertexAttributeDescriptions     = mVkVertexInputAttribute;
}

int
ShaderProgram::GetInfoLogLength(void) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    return mShaderCompiler->GetProgramInfoLog(ESSL_VERSION_100) ? (int)strlen(mShaderCompiler->GetProgramInfoLog(ESSL_VERSION_100)) + 1 : 0;
}

Shader *
ShaderProgram::IsShaderAttached(Shader *shader) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(shader &&
      ((shader->GetShaderType() == SHADER_TYPE_VERTEX   && mShaders[0] == shader) ||
       (shader->GetShaderType() == SHADER_TYPE_FRAGMENT && mShaders[1] == shader))) {
        return shader;
    }

    return nullptr;
}

void
ShaderProgram::AttachShader(Shader *shader)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    shader->Bind();
    mShaders[shader->GetShaderType() == SHADER_TYPE_VERTEX ? 0 : 1] = shader;
}

void
ShaderProgram::DetachShader(Shader *shader)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(!shader || !IsShaderAttached(shader)) {
        return;
    }

    if(shader->GetShaderType() == SHADER_TYPE_VERTEX) {
        mShaders[0] = nullptr;
    } else if(shader->GetShaderType() == SHADER_TYPE_FRAGMENT) {
        mShaders[1] = nullptr;
    }

    shader->Unbind();
}

void
ShaderProgram::DetachShaders()
{
    FUN_ENTRY(GL_LOG_DEBUG);

    Shader *shaderPtr;

    shaderPtr = GetVertexShader();
    if(shaderPtr) {
       DetachShader(shaderPtr);
    }

    shaderPtr = GetFragmentShader();
    if(shaderPtr) {
        DetachShader(shaderPtr);
    }
}

uint32_t
ShaderProgram::SerializeShadersSpirv(void *binary)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    uint8_t *rawDataPtr = reinterpret_cast<uint8_t *>(binary);
    uint32_t *u32DataPtr = nullptr;
    uint32_t vsSpirvSize = static_cast<int>(4 * mShaderSPVsize[0]);
    uint32_t fsSpirvSize = static_cast<int>(4 * mShaderSPVsize[1]);

    u32DataPtr = reinterpret_cast<uint32_t *>(rawDataPtr);
    *u32DataPtr = vsSpirvSize;
    rawDataPtr += sizeof(uint32_t);
    memcpy(rawDataPtr, mShaderSPVdata[0], vsSpirvSize);
    rawDataPtr += vsSpirvSize;

    u32DataPtr = reinterpret_cast<uint32_t *>(rawDataPtr);
    *u32DataPtr = fsSpirvSize;
    rawDataPtr += sizeof(uint32_t);
    memcpy(rawDataPtr, mShaderSPVdata[1], fsSpirvSize);

    return 2 * sizeof(uint32_t) + vsSpirvSize + fsSpirvSize;
}

uint32_t
ShaderProgram::DeserializeShadersSpirv(const void *binary)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    const uint8_t *rawDataPtr = reinterpret_cast<const uint8_t *>(binary);
    const uint32_t *u32DataPtr = nullptr;
    std::vector<uint32_t> &vsSpirvData = GetVertexShader()->GetSPV();
    std::vector<uint32_t> &fsSpirvData = GetFragmentShader()->GetSPV();
    uint32_t vsSpirvSize = 0;
    uint32_t fsSpirvSize = 0;

    u32DataPtr = reinterpret_cast<const uint32_t *>(rawDataPtr);
    vsSpirvSize = *u32DataPtr;\
    rawDataPtr += sizeof(uint32_t);
    u32DataPtr = reinterpret_cast<const uint32_t *>(rawDataPtr);
    std::copy(u32DataPtr, u32DataPtr + vsSpirvSize /4, back_inserter(vsSpirvData));
    rawDataPtr += vsSpirvSize;

    u32DataPtr = reinterpret_cast<const uint32_t *>(rawDataPtr);
    fsSpirvSize = *u32DataPtr;
    rawDataPtr += sizeof(uint32_t);
    u32DataPtr = reinterpret_cast<const uint32_t *>(rawDataPtr);
    std::copy(u32DataPtr, u32DataPtr + fsSpirvSize /4, back_inserter(fsSpirvData));
    rawDataPtr += fsSpirvSize;

    return 2 * sizeof(uint32_t) + vsSpirvSize + fsSpirvSize;
}

const ShaderResourceInterface::attribute *
ShaderProgram::GetVertexAttribute(int index) const
{
    FUN_ENTRY(GL_LOG_TRACE);

    return mShaderResourceInterface.GetVertexAttribute(index);
}

uint32_t
ShaderProgram::GetNumberOfActiveAttributes(void) const
{
    FUN_ENTRY(GL_LOG_TRACE);

    return mShaderResourceInterface.GetLiveAttributes();
}

int
ShaderProgram::GetAttributeType(int index) const
{
    FUN_ENTRY(GL_LOG_TRACE);

    return mShaderResourceInterface.GetAttributeType(index);
}

int
ShaderProgram::GetAttributeLocation(const char *name) const
{
    FUN_ENTRY(GL_LOG_TRACE);

    return mShaderResourceInterface.GetAttributeLocation(name);
}

VkPipelineCache
ShaderProgram::GetVkPipelineCache(void)
{
    FUN_ENTRY(GL_LOG_TRACE);

    if(mPipelineCache->GetPipelineCache() == VK_NULL_HANDLE) {
        mPipelineCache->Create(nullptr, 0);
    }

    return mPipelineCache->GetPipelineCache();
}

const std::string&
ShaderProgram::GetAttributeName(int index) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    return mShaderResourceInterface.GetAttributeName(index);
}

void
ShaderProgram::Validate()
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mValidated = true;

    if(!mLinked) {
        mValidated = false;
        return;
    }
}

bool
ShaderProgram::ValidateProgram(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    
    const Shader *vs = mShaders[0];
    const Shader *fs = mShaders[1];

    if((!vs || !fs) ||
       (!vs->IsCompiled() || !fs->IsCompiled())) {
        return false;
    }

    if(GLOVE_DUMP_INPUT_SHADER_REFLECTION) {
        mShaderCompiler->EnablePrintReflection(ESSL_VERSION_100);
    }
    
    return mShaderCompiler->ValidateProgram(ESSL_VERSION_100);
}

bool
ShaderProgram::LinkProgram()
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(!(mLinked = ValidateProgram())) {
        return false;
    }

    if(GLOVE_SAVE_SHADER_SOURCES_TO_FILES) {
        mShaderCompiler->EnableSaveSourceToFiles();
    }

    if(GLOVE_SAVE_SPIRV_BINARY_TO_FILES) {
        mShaderCompiler->EnableSaveBinaryToFiles();
    }

    if(GLOVE_SAVE_SPIRV_TEXT_TO_FILE) {
        mShaderCompiler->EnableSaveSpvTextToFile();
    }

    if(GLOVE_DUMP_PROCESSED_SHADER_SOURCE) {
        mShaderCompiler->EnablePrintConvertedShader();
    }

    if(GLOVE_DUMP_VULKAN_SHADER_REFLECTION) {
        mShaderCompiler->EnablePrintReflection(ESSL_VERSION_400);
    }

    if(GLOVE_DUMP_SPIRV_SHADER_SOURCE) {
        mShaderCompiler->EnablePrintSpv();
    }

    ResetVulkanVertexInput();

    mShaderCompiler->PrepareReflection(ESSL_VERSION_100);
    UpdateAttributeInterface();

    Context *context = GetCurrentContext();
    assert(context);
    mLinked = mShaderCompiler->PreprocessShader((uintptr_t)this, SHADER_TYPE_VERTEX  , ESSL_VERSION_100, ESSL_VERSION_400, context->IsYInverted()) &&
              mShaderCompiler->PreprocessShader((uintptr_t)this, SHADER_TYPE_FRAGMENT, ESSL_VERSION_100, ESSL_VERSION_400, context->IsYInverted());
    if(!mLinked) {
        return false;
    }
    mLinked = mShaderCompiler->LinkProgram((uintptr_t)this, ESSL_VERSION_400, GetVertexShader()->GetSPV(), GetFragmentShader()->GetSPV());
    if(!mLinked) {
        return false;
    }
    BuildShaderResourceInterface();

    /// A program object will fail to link if the number of active vertex attributes exceeds GL_MAX_VERTEX_ATTRIBS
    /// A link error will be generated if an attempt is made to utilize more than the space available for fragment shader uniform variables.
    if(GetNumberOfActiveUniforms() > GLOVE_MAX_VERTEX_UNIFORM_VECTORS ||
       GetNumberOfActiveUniforms() > GLOVE_MAX_FRAGMENT_UNIFORM_VECTORS ||
       GetNumberOfActiveAttributes() > GLOVE_MAX_VERTEX_ATTRIBS) {
        mLinked = false;
        return false;
    }

    if(GLOVE_DUMP_VULKAN_SHADER_REFLECTION) {
        printf("-------- SHADER PROGRAM REFLECTION GLOVE --------\n\n");
        mShaderCompiler->PrintUniformReflection();
        printf("-------------------------------------------------\n\n");
    }

    return mLinked;
}

bool
ShaderProgram::AllocateExplicitIndexBuffer(const void* data, size_t size, BufferObject** ibo)
{
    FUN_ENTRY(GL_LOG_TRACE);

    if(mExplicitIbo != nullptr) {
        mCacheManager->CacheVBO(mExplicitIbo);
        mExplicitIbo = nullptr;
    }

    mExplicitIbo = new IndexBufferObject(mVkContext);
    mExplicitIbo->SetTarget(GL_ELEMENT_ARRAY_BUFFER);
    *ibo = mExplicitIbo;

    return mExplicitIbo->Allocate(size, data);
}

bool
ShaderProgram::ConvertIndexBufferToUint16(const void* srcData, size_t elementCount, BufferObject** ibo)
{
    FUN_ENTRY(GL_LOG_TRACE);

    uint16_t* convertedIndicesU16 = new uint16_t[elementCount];
    size_t actualSize = elementCount * sizeof(uint16_t);

    bool validatedBuffer = ConvertBuffer<uint8_t, uint16_t>(srcData, convertedIndicesU16, elementCount);
    if(validatedBuffer) {
        validatedBuffer = AllocateExplicitIndexBuffer(convertedIndicesU16, actualSize, ibo);
    }
    delete[] convertedIndicesU16;

    return validatedBuffer;
}

void
ShaderProgram::LineLoopConversion(void* data, uint32_t indexCount, size_t elementByteSize)
{
    FUN_ENTRY(GL_LOG_TRACE);

    memcpy(static_cast<uint8_t*>(data) + (indexCount - 1) * elementByteSize, data, elementByteSize);
}

uint32_t
ShaderProgram::GetMaxIndex(BufferObject* ibo, uint32_t indexCount, size_t actualSize, VkDeviceSize offset)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    uint16_t* srcData = new uint16_t[actualSize];
    ibo->GetData(actualSize, offset, srcData);

    uint16_t maxIndex = srcData[0];
    for(uint32_t i = indexCount - 1; i > 0; --i) {
        uint16_t index = srcData[i];
        if(maxIndex < index) {
            maxIndex = index;
        }
    }
    delete[] srcData;

    return maxIndex;
}

void
ShaderProgram::PrepareIndexBufferObject(uint32_t* firstIndex, uint32_t* maxIndex, uint32_t indexCount, GLenum type, const void* indices, BufferObject* ibo)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mActiveIndexVkBuffer = VK_NULL_HANDLE;
    size_t actualSize = indexCount * (type == GL_UNSIGNED_INT ? sizeof(GLuint) : sizeof(GLushort));
    VkDeviceSize offset = 0;
    bool validatedBuffer = true;

    // Index buffer requires special handling for passing data and handling unsigned bytes:
    // - If there is a index buffer bound, use the indices parameter as offset.
    // - Otherwise, indices contains the index buffer data. Therefore create a temporary object and store the data there.
    // If the data format is GL_UNSIGNED_BYTE (not supported by Vulkan), convert the data to uint16 and pass this instead.
    if(ibo) {
        offset = reinterpret_cast<VkDeviceSize>(indices);

        if(type == GL_UNSIGNED_BYTE) {
            assert(indexCount <= ibo->GetSize());
            uint8_t* srcData = new uint8_t[indexCount];
            ibo->GetData(indexCount, offset, srcData);
            offset = 0;
            validatedBuffer = ConvertIndexBufferToUint16(srcData, indexCount, &ibo);
            delete[] srcData;
        }
    } else {
        if(type == GL_UNSIGNED_BYTE) {
            validatedBuffer = ConvertIndexBufferToUint16(indices, indexCount, &ibo);
        } else {
            validatedBuffer = AllocateExplicitIndexBuffer(indices, actualSize, &ibo);
        }
    }

    assert(GetCurrentContext());
    if(GetCurrentContext()->IsModeLineLoop()) {
        size_t sizeOne = type == GL_UNSIGNED_INT ? sizeof(GLuint) : sizeof(GLushort);
        uint8_t* srcData = new uint8_t[indexCount * sizeOne];

        ibo->GetData(actualSize - sizeOne, offset, srcData);
        LineLoopConversion(srcData, indexCount, sizeOne);

        validatedBuffer = AllocateExplicitIndexBuffer(srcData, actualSize, &ibo);
        delete[] srcData;
    }

    if(validatedBuffer) {
        *firstIndex = offset;
        *maxIndex = GetMaxIndex(ibo, indexCount, actualSize, offset);
        mActiveIndexVkBuffer = ibo->GetVkBuffer();
    }
}

bool
ShaderProgram::PrepareVertexAttribBufferObjects(size_t vertCount, uint32_t firstVertex,
                                                std::vector<GenericVertexAttribute>& genericVertAttribs,
                                                bool updatedVertexAttrib)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    // store the location-binding associations for faster lookup
    std::map<uint32_t, uint32_t> vboLocationBindings;

    if(UpdateVertexAttribProperties(vertCount, firstVertex, genericVertAttribs, vboLocationBindings, updatedVertexAttrib)) {
        GenerateVertexInputProperties(genericVertAttribs, vboLocationBindings);
        return true;
    }
    return false;
}

bool
ShaderProgram::UpdateVertexAttribProperties(size_t vertCount, uint32_t firstVertex,
                                              std::vector<GenericVertexAttribute>& genericVertAttribs,
                                              std::map<uint32_t, uint32_t>& vboLocationBindings, bool updatedVertexAttrib)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    assert(GetCurrentContext());
    if(GetCurrentContext()->IsModeLineLoop()) {
        --vertCount;
    }

    // store attribute locations containing the same VkBuffer and stride
    // as they are directly associated with vertex input bindings
    typedef std::pair<VkBuffer, int32_t> BUFFER_STRIDE_PAIR;
    std::map<BUFFER_STRIDE_PAIR, std::vector<uint32_t>> unique_buffer_stride_map;

    std::vector<uint32_t> locationUsed;
    for(uint32_t i = 0; i < mShaderResourceInterface.GetLiveAttributes(); ++i) {
        const uint32_t attributelocation  = mShaderResourceInterface.GetAttributeLocation(i);
        const uint32_t occupiedLocations = OccupiedLocationsPerGlType(mShaderResourceInterface.GetAttributeType(i));

        for(uint32_t j = 0; j < occupiedLocations; ++j) {
            const uint32_t location = attributelocation + j;

            // if location is currently used then ommit it
            if (std::find(locationUsed.begin(), locationUsed.end(), location) != locationUsed.end()) {
                continue;
            }

            GenericVertexAttribute& gva = genericVertAttribs[location];
            bool updatedVBO   = false;
            BufferObject *vbo = gva.UpdateVertexAttribute(static_cast<uint32_t>(firstVertex + vertCount), updatedVBO);
            if(updatedVBO) {
                updatedVertexAttrib = true;
            }
            VkBuffer bo       = vbo->GetVkBuffer();

            // If the primitives are rendered with GL_LINE_LOOP, which is not
            // supported in Vulkan, we have to modify the vbo and add the first vertex at the end.
            if(GetCurrentContext()->IsModeLineLoop() && !mActiveIndexVkBuffer) {
                BufferObject* vboLineLoopUpdated = new VertexBufferObject(mVkContext);

                size_t sizeOld = vbo->GetSize();
                size_t sizeOne = gva.GetStride();
                size_t sizeNew = sizeOld + sizeOne;

                uint8_t *dataNew = new uint8_t[sizeNew];

                vbo->GetData(sizeOld, 0, dataNew);
                memcpy(dataNew + sizeOld, dataNew, sizeOne);
                vboLineLoopUpdated->Allocate(sizeNew, dataNew);

                delete[] dataNew;
                bo          = vboLineLoopUpdated->GetVkBuffer();
                mCacheManager->CacheVBO(vboLineLoopUpdated);
                updatedVertexAttrib = true;
            }

            // store each location
            int32_t stride      = gva.GetStride();
            BUFFER_STRIDE_PAIR p = {bo, stride};
            unique_buffer_stride_map[p].push_back(location);
            locationUsed.push_back(location);
        }
    }

    if(!updatedVertexAttrib) {
        return false;
    }

    memset(mActiveVertexVkBuffers, VK_NULL_HANDLE, sizeof(VkBuffer) * mActiveVertexVkBuffersCount);
    mActiveVertexVkBuffersCount = 0;

    // generate unique bindings for each VKbuffer/stride pair
    uint32_t current_binding = 0;
    for(const auto& iter : unique_buffer_stride_map) {
        VkBuffer bo = iter.first.first;
        for(const auto& loc_str_iter : iter.second) {
            vboLocationBindings[loc_str_iter] = current_binding;
        }
        mActiveVertexVkBuffers[current_binding] = bo;
        ++current_binding;
    }
    mActiveVertexVkBuffersCount = current_binding;
    return true;
}

void
ShaderProgram::GenerateVertexInputProperties(std::vector<GenericVertexAttribute>& genericVertAttribs, const std::map<uint32_t, uint32_t>& vboLocationBindings)
{
    // create vertex input bindings and attributes
    uint32_t count = 0;
    std::vector<uint32_t> locationUsed;

    for(uint32_t i = 0; i < mShaderResourceInterface.GetLiveAttributes(); ++i) {
        const uint32_t attributelocation  = mShaderResourceInterface.GetAttributeLocation(i);
        const uint32_t occupiedLocations = OccupiedLocationsPerGlType(mShaderResourceInterface.GetAttributeType(i));

        for(uint32_t j = 0; j < occupiedLocations; ++j) {
            const uint32_t location = attributelocation + j;
            const uint32_t binding  = vboLocationBindings.at(location);

            // if location is currently used then ommit it
            if (std::find(locationUsed.begin(), locationUsed.end(), location) != locationUsed.end()) {
                continue;
            }

            GenericVertexAttribute& gva = genericVertAttribs[location];
            mVkVertexInputBinding[binding].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            mVkVertexInputBinding[binding].binding   = binding;
            mVkVertexInputBinding[binding].stride    = static_cast<uint32_t>(gva.GetStride());

            mVkVertexInputAttribute[count].binding  = binding;
            mVkVertexInputAttribute[count].location = location;
            mVkVertexInputAttribute[count].format   = gva.GetVkFormat();
            mVkVertexInputAttribute[count].offset   = gva.GetOffset();

            ++count;

            locationUsed.push_back(location);
        }
    }

    mVkPipelineVertexInput.vertexBindingDescriptionCount   = mActiveVertexVkBuffersCount;
    mVkPipelineVertexInput.vertexAttributeDescriptionCount = count;
}

void
ShaderProgram::UsePrecompiledBinary(const void *binary, size_t binarySize)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mLinked = true;

    ResetVulkanVertexInput();

    uint32_t reflectionOffset = mShaderCompiler->DeserializeReflection(binary);
    uint32_t spirvOffset = DeserializeShadersSpirv(reinterpret_cast<const uint8_t *>(binary) + reflectionOffset);
    const uint8_t *vulkanDataPtr = reinterpret_cast<const uint8_t *>(binary) + reflectionOffset + spirvOffset;

    BuildShaderResourceInterface();

    mPipelineCache->Create(vulkanDataPtr, binarySize - reflectionOffset);

    mIsPrecompiled = true;
}

void
ShaderProgram::GetBinaryData(void *binary, GLsizei *binarySize)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    uint32_t reflectionOffset = mShaderCompiler->SerializeReflection(binary);

    uint8_t *spirvDataPtr = reinterpret_cast<uint8_t *>(binary) + reflectionOffset;
    uint32_t spirvOffset = SerializeShadersSpirv(spirvDataPtr);

    uint8_t *vulkanDataPtr = reinterpret_cast<uint8_t *>(binary) + reflectionOffset + spirvOffset;
    size_t vulkanDataSize = *binarySize;

    if(mPipelineCache->GetPipelineCache() != VK_NULL_HANDLE) {
        mPipelineCache->GetData(reinterpret_cast<void *>(vulkanDataPtr), &vulkanDataSize);
        *binarySize = vulkanDataSize + reflectionOffset + spirvOffset;
    } else {
        *binarySize = 0;
    }
}

GLsizei
ShaderProgram::GetBinaryLength(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    size_t vkPipelineCacheDataLength = 0;
    uint32_t spirvSize = 2 * sizeof(uint32_t) + 4 * (mShaderSPVsize[0] + mShaderSPVsize[1]);

    if(mPipelineCache->GetPipelineCache() != VK_NULL_HANDLE) {
        mPipelineCache->GetData(nullptr, &vkPipelineCacheDataLength);
    }

    return vkPipelineCacheDataLength + mShaderResourceInterface.GetReflectionSize() + spirvSize;
}

char *
ShaderProgram::GetInfoLog(void) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    char *log = nullptr;

    if(mShaderCompiler) {
        uint32_t len = strlen(mShaderCompiler->GetProgramInfoLog(ESSL_VERSION_100)) + 1;
        log = new char[len];

        memcpy(log, mShaderCompiler->GetProgramInfoLog(ESSL_VERSION_100), len);
        log[len - 1] = '\0';
    }

    return log;
}

void
ShaderProgram::GetUniformData(uint32_t location, size_t size, void *ptr) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mShaderResourceInterface.GetUniformClientData(location, size, ptr);
}

void
ShaderProgram::SetUniformData(uint32_t location, size_t size, const void *ptr)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mShaderResourceInterface.SetUniformClientData(location, size, ptr);
    mUpdateDescriptorData = true;
}

void
ShaderProgram::SetUniformSampler(uint32_t location, int count, const int *textureUnit)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mShaderResourceInterface.SetUniformSampler(location, count, textureUnit);
    mUpdateDescriptorSets = true;
}

void
ShaderProgram::SetCacheManager(CacheManager *cacheManager)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mCacheManager = cacheManager;
    mShaderResourceInterface.SetCacheManager(cacheManager);
}

void
ShaderProgram::ReleaseVkObjects(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(mVkPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(mVkContext->vkDevice, mVkPipelineLayout, nullptr);
        mVkPipelineLayout = VK_NULL_HANDLE;
    }

    if(mVkDescSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(mVkContext->vkDevice, mVkDescSetLayout, nullptr);
        mVkDescSetLayout = VK_NULL_HANDLE;
    }

    if(mVkDescSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(mVkContext->vkDevice, mVkDescPool, 1, &mVkDescSet);
        mVkDescSet = VK_NULL_HANDLE;
    }

    if(mVkDescPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mVkContext->vkDevice, mVkDescPool, nullptr);
        mVkDescPool = VK_NULL_HANDLE;
    }

    for(int32_t i = 0; i < MAX_SHADERS; ++i) {
        mShaderSPVsize[i] = 0;
        mShaderSPVdata[i] = nullptr;
        mVkShaderModules[i] = VK_NULL_HANDLE;
        mVkShaderStages[i] = VK_SHADER_STAGE_ALL;
    }

    mPipelineCache->Release();
}

void
ShaderProgram::SetShaderModules(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mStageCount = HasVertexShader() + HasFragmentShader();
    assert(mStageCount == 0 || mStageCount == 1 || mStageCount == 2);

    if(mStageCount == 1) {

        Shader* shader = HasVertexShader() ? GetVertexShader() : GetFragmentShader();

        mVkShaderModules[0] = shader->CreateVkShaderModule();
        mVkShaderStages[0]  = HasVertexShader() ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;

    } else if(mStageCount == 2) {

        Shader* shader = GetVertexShader();

        mVkShaderModules[0] = shader->CreateVkShaderModule();
        mShaderSPVsize[0]  = shader->GetSPV().size();
        mShaderSPVdata[0]  = shader->GetSPV().data();
        mVkShaderStages[0] = VK_SHADER_STAGE_VERTEX_BIT;

        shader = GetFragmentShader();

        mVkShaderModules[1] = shader->CreateVkShaderModule();
        mShaderSPVsize[1]  = shader->GetSPV().size();
        mShaderSPVdata[1]  = shader->GetSPV().data();
        mVkShaderStages[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
}

bool
ShaderProgram::CreateDescriptorSetLayout(uint32_t nLiveUniformBlocks)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(nLiveUniformBlocks) {
        mVkDescSetLayoutBind = new VkDescriptorSetLayoutBinding[nLiveUniformBlocks];
        assert(mVkDescSetLayoutBind);

        for(uint32_t i = 0; i < mShaderResourceInterface.GetLiveUniformBlocks(); ++i) {
            mVkDescSetLayoutBind[i].binding = mShaderResourceInterface.GetUniformBlockBinding(i);
            mVkDescSetLayoutBind[i].descriptorType = mShaderResourceInterface.IsUniformBlockOpaque(i) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            mVkDescSetLayoutBind[i].descriptorCount = 1;
            mVkDescSetLayoutBind[i].stageFlags = mShaderResourceInterface.GetUniformBlockStage(i) == (SHADER_TYPE_VERTEX | SHADER_TYPE_FRAGMENT) ? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT :
                                                 mShaderResourceInterface.GetUniformBlockStage(i) ==  SHADER_TYPE_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
            mVkDescSetLayoutBind[i].pImmutableSamplers = nullptr;
        }
    }

    VkDescriptorSetLayoutCreateInfo descLayoutInfo;
    memset(static_cast<void *>(&descLayoutInfo), 0, sizeof(descLayoutInfo));
    descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutInfo.pNext = nullptr;
    descLayoutInfo.flags = 0;
    descLayoutInfo.bindingCount = nLiveUniformBlocks;
    descLayoutInfo.pBindings = mVkDescSetLayoutBind;

    if(vkCreateDescriptorSetLayout(mVkContext->vkDevice, &descLayoutInfo, 0, &mVkDescSetLayout) != VK_SUCCESS) {
        assert(0);
        return false;
    }
    assert(mVkDescSetLayout != VK_NULL_HANDLE);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    memset(static_cast<void *>(&pipelineLayoutCreateInfo), 0, sizeof(pipelineLayoutCreateInfo));
    pipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext                  = nullptr;
    pipelineLayoutCreateInfo.flags                  = 0;
    pipelineLayoutCreateInfo.setLayoutCount         = 1;
    pipelineLayoutCreateInfo.pSetLayouts            = &mVkDescSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges    = nullptr;

    if(vkCreatePipelineLayout(mVkContext->vkDevice, &pipelineLayoutCreateInfo, 0, &mVkPipelineLayout) != VK_SUCCESS) {
        assert(0);
        return false;
    }
    
    if(nLiveUniformBlocks) {
        delete[] mVkDescSetLayoutBind;
        mVkDescSetLayoutBind = nullptr;
    }

    return true;
}

bool
ShaderProgram::CreateDescriptorPool(uint32_t nLiveUniformBlocks)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    VkDescriptorPoolSize *descTypeCounts = new VkDescriptorPoolSize[nLiveUniformBlocks];
    assert(descTypeCounts);
    memset(static_cast<void *>(descTypeCounts), 0, nLiveUniformBlocks * sizeof(*descTypeCounts));

    for(uint32_t i = 0; i < mShaderResourceInterface.GetLiveUniformBlocks(); ++i) {
        descTypeCounts[i].descriptorCount = 1;
        descTypeCounts[i].type = mShaderResourceInterface.IsUniformBlockOpaque(i) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    VkDescriptorPoolCreateInfo descriptorPoolInfo;
    memset(static_cast<void *>(&descriptorPoolInfo), 0, sizeof(descriptorPoolInfo));
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.poolSizeCount = nLiveUniformBlocks;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.maxSets = 1;
    descriptorPoolInfo.pPoolSizes = descTypeCounts;

    if(vkCreateDescriptorPool(mVkContext->vkDevice, &descriptorPoolInfo, 0, &mVkDescPool) != VK_SUCCESS) {
        assert(0);
        return false;
    }
    assert(mVkDescPool != VK_NULL_HANDLE);

    delete[] descTypeCounts;

    return true;
}

bool
ShaderProgram::CreateDescriptorSet(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    VkDescriptorSetAllocateInfo descAllocInfo;
    memset(static_cast<void *>(&descAllocInfo), 0, sizeof(descAllocInfo));
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.pNext = nullptr;
    descAllocInfo.descriptorPool = mVkDescPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &mVkDescSetLayout;

    if(vkAllocateDescriptorSets(mVkContext->vkDevice, &descAllocInfo, &mVkDescSet) != VK_SUCCESS) {
        assert(0);
        return false;
    }
    assert(mVkDescSet != VK_NULL_HANDLE);

    return true;
}

bool
ShaderProgram::AllocateVkDescriptoSet(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    const uint32_t nLiveUniformBlocks = mShaderResourceInterface.GetLiveUniformBlocks();

    ReleaseVkObjects();

    if(!CreateDescriptorSetLayout(nLiveUniformBlocks)) {
        assert(0);
        return false;
    }

    if(!nLiveUniformBlocks) {
        return true;
    }

    if(!CreateDescriptorPool(nLiveUniformBlocks)) {
        assert(0);
        return false;
    }

    if(!CreateDescriptorSet()) {
        assert(0);
        return false;
    }

    return true;
}

void
ShaderProgram::UpdateBuiltInUniformData(float minDepthRange, float maxDepthRange)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(mMinDepthRange == minDepthRange && mMaxDepthRange == maxDepthRange) {
        return;
    }

    mMinDepthRange = minDepthRange;
    mMaxDepthRange = maxDepthRange;

    int location;
    if((location = GetUniformLocation("gl_DepthRange.near")) != -1) {
        SetUniformData(location, sizeof(float), &mMinDepthRange);
    }

    if((location = GetUniformLocation("gl_DepthRange.far")) != -1) {
        SetUniformData(location, sizeof(float), &mMaxDepthRange);
    }

    if((location = GetUniformLocation("gl_DepthRange.diff")) != -1) {
        float diffDepthRange = mMaxDepthRange - mMinDepthRange;
        SetUniformData(location, sizeof(float), &diffDepthRange);
    }
}

void
ShaderProgram::UpdateDescriptorSet(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    Context *context = GetCurrentContext();
    assert(context);
    assert(mVkDescSet);
    assert(mVkContext);

    if(mShaderResourceInterface.GetLiveUniformBlocks() == 0) {
        return;
    }

    /// Transfer any new local uniform data into the buffer objects
    if(mUpdateDescriptorData) {
        bool allocatedNewBufferObject = false;
        mShaderResourceInterface.UpdateUniformBufferData(mVkContext, &allocatedNewBufferObject);
        if(allocatedNewBufferObject) {
            mUpdateDescriptorSets = true;
        }

        mUpdateDescriptorData = false;
    }

    // Check if any texture is attached to a user-based FBO
    for(uint32_t i = 0; i < mShaderResourceInterface.GetLiveUniforms(); ++i) {
        if(mShaderResourceInterface.GetUniformType(i) == GL_SAMPLER_2D || mShaderResourceInterface.GetUniformType(i) == GL_SAMPLER_CUBE) {
            for(int32_t j = 0; j < mShaderResourceInterface.GetUniformArraySize(i); ++j) {
                const glsl_sampler_t textureUnit = *(glsl_sampler_t *)mShaderResourceInterface.GetUniformClientData(i);

                /// Sampler might need an update
                Texture *activeTexture = context->GetStateManager()->GetActiveObjectsState()->GetActiveTexture(
                mShaderResourceInterface.GetUniformType(i) == GL_SAMPLER_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP, textureUnit); // TODO remove mGlContext
                if(context->GetResourceManager()->IsTextureAttachedToFBO(activeTexture)) {
                    mUpdateDescriptorSets = true;
                    break;
                }
            }
        }
    }

    /// This can be true only in three occasions:
    /// 1. This is a freshly linked shader. So the descriptor sets need to be created
    /// 2. There has been an update in a sampler via the glUniform1i()
    /// 3. glBindTexture has been called
    /// 4. Texture is attached to a user-based FBO
    if(!mUpdateDescriptorSets) {
        return;
    }

    UpdateSamplerDescriptors();

    mUpdateDescriptorSets = false;
}

void
ShaderProgram::UpdateSamplerDescriptors(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    Context *context = GetCurrentContext();
    assert(context);

    const uint32_t nLiveUniformBlocks = mShaderResourceInterface.GetLiveUniformBlocks();
    uint32_t nSamplers = 0;
    for(uint32_t i = 0; i < nLiveUniformBlocks; ++i) {
        if(mShaderResourceInterface.IsUniformBlockOpaque(i)) {
            nSamplers += mShaderResourceInterface.GetUniformArraySize(i);
        }
    }

    /// Get texture units from samplers
    uint32_t samp = 0;
    std::map<uint32_t, uint32_t> map_block_texDescriptor;
    VkDescriptorImageInfo *textureDescriptors = nullptr;
    if(nSamplers) {
        textureDescriptors = new VkDescriptorImageInfo[nSamplers];
        memset(static_cast<void *>(textureDescriptors), 0, nSamplers * sizeof(*textureDescriptors));

        for(uint32_t i = 0; i < mShaderResourceInterface.GetLiveUniforms(); ++i) {
            if(mShaderResourceInterface.GetUniformType(i) == GL_SAMPLER_2D || mShaderResourceInterface.GetUniformType(i) == GL_SAMPLER_CUBE) {
                for(int32_t j = 0; j < mShaderResourceInterface.GetUniformArraySize(i); ++j) {
                    const glsl_sampler_t textureUnit = *(glsl_sampler_t *)mShaderResourceInterface.GetUniformClientData(i);

                    /// Sampler might need an update
                    Texture *activeTexture = context->GetStateManager()->GetActiveObjectsState()->GetActiveTexture(
                    mShaderResourceInterface.GetUniformType(i) == GL_SAMPLER_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP, textureUnit); // TODO remove mGlContext
                    // Calling a sampler from a fragment shader must return (0, 0, 0, 1) 
                    // when the sampler’s associated texture object is not complete.
                    if( !activeTexture->IsCompleted() || !activeTexture->IsNPOTAccessCompleted()) {
                        uint8_t pixels[4] = {0,0,0,255};
                        for(GLint layer = 0; layer < activeTexture->GetLayersCount(); ++layer) {
                            for(GLint level = 0; level < activeTexture->GetMipLevelsCount(); ++level) {
                                activeTexture->SetState(1, 1, level, layer, GL_RGBA, GL_UNSIGNED_BYTE, Texture::GetDefaultInternalAlignment(), pixels);
                            }
                        }

                        if(activeTexture->IsCompleted()) {
                            activeTexture->SetVkFormat(VK_FORMAT_R8G8B8A8_UNORM);
                            activeTexture->Allocate();
                            activeTexture->PrepareVkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                        }
                    }
                    else if(context->GetResourceManager()->IsTextureAttachedToFBO(activeTexture)) {

                        // Get Inverted Data from FBO's Color Attachment Texture
                        GLenum dstInternalFormat = activeTexture->GetExplicitInternalFormat();
                        ImageRect srcRect(0, 0, activeTexture->GetWidth(), activeTexture->GetHeight(),
                            GlInternalFormatTypeToNumElements(dstInternalFormat, activeTexture->GetExplicitType()),
                            GlTypeToElementSize(activeTexture->GetExplicitType()),
                            Texture::GetDefaultInternalAlignment());
                        ImageRect dstRect(0, 0, activeTexture->GetWidth(), activeTexture->GetHeight(),
                            GlInternalFormatTypeToNumElements(dstInternalFormat, activeTexture->GetExplicitType()),
                            GlTypeToElementSize(activeTexture->GetExplicitType()),
                            Texture::GetDefaultInternalAlignment());

                        uint8_t* dstData = new uint8_t[dstRect.GetRectBufferSize()];
                        srcRect.y = activeTexture->GetInvertedYOrigin(&srcRect);
                        activeTexture->CopyPixelsToHost  (&srcRect, &dstRect, 0, 0, dstInternalFormat, static_cast<void *>(dstData));

                        // Create new Texture with this data 
                        Texture *inverted_texture = new Texture(mVkContext);
                        inverted_texture->SetTarget(GL_TEXTURE_2D);
                        inverted_texture->SetVkImageUsage(static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
                        inverted_texture->SetVkImageTiling();
                        inverted_texture->SetVkImageTarget(vulkanAPI::Image::VK_IMAGE_TARGET_2D);
                        inverted_texture->InitState();

                        inverted_texture->SetVkFormat(activeTexture->GetVkFormat());
                        inverted_texture->SetState(activeTexture->GetWidth(), activeTexture->GetHeight(),
                                    0, 0,
                                    GlInternalFormatToGlFormat(dstInternalFormat),
                                    GlInternalFormatToGlType(dstInternalFormat),
                                    Texture::GetDefaultInternalAlignment(),
                                    dstData);
                        
                        if(inverted_texture->IsCompleted()) {
                            inverted_texture->Allocate();
                            inverted_texture->PrepareVkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                            mCacheManager->CacheTexture(inverted_texture);
                        }

                        activeTexture = inverted_texture;

                        delete[] dstData;
                    }

                    activeTexture->CreateVkSampler();

                    textureDescriptors[samp].sampler     = activeTexture->GetVkSampler();
                    textureDescriptors[samp].imageLayout = activeTexture->GetVkImageLayout();
                    textureDescriptors[samp].imageView   = activeTexture->GetVkImageView();

                    if(j == 0) {
                        map_block_texDescriptor[mShaderResourceInterface.GetUniformBlockIndex(i)] = samp;
                    }
                    ++samp;
                }
            }
        }
    }
    assert(samp == nSamplers);

    VkWriteDescriptorSet *writes = new VkWriteDescriptorSet[nLiveUniformBlocks];
    memset(static_cast<void*>(writes), 0, nLiveUniformBlocks * sizeof(*writes));
    for(uint32_t i = 0; i < nLiveUniformBlocks; ++i) {
        writes[i].sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext      = nullptr;
        writes[i].dstSet     = mVkDescSet;
        writes[i].dstBinding = mShaderResourceInterface.GetUniformBlockBinding(i);

        if(mShaderResourceInterface.IsUniformBlockOpaque(i)) {
            writes[i].pImageInfo      = &textureDescriptors[map_block_texDescriptor[i]];
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].descriptorCount = mShaderResourceInterface.GetUniformArraySize(i);
        } else {
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[i].pBufferInfo     = mShaderResourceInterface.GetUniformBufferObject(i)->GetBufferDescInfo();
        }
    }

    vkUpdateDescriptorSets(mVkContext->vkDevice, nLiveUniformBlocks, writes, 0, nullptr);

    delete[] writes;
    delete[] textureDescriptors;

    mUpdateDescriptorSets = false;
}

void
ShaderProgram::ResetVulkanVertexInput(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mVkPipelineVertexInput.vertexAttributeDescriptionCount = 0;
    mVkPipelineVertexInput.vertexBindingDescriptionCount = 0;
    mActiveVertexVkBuffersCount = 0;
    memset(static_cast<void *>(mActiveVertexVkBuffers), 0, sizeof(mActiveVertexVkBuffers));
}

void
ShaderProgram::UpdateAttributeInterface(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mShaderResourceInterface.SetReflection(mShaderCompiler->GetShaderReflection());
    mShaderResourceInterface.UpdateAttributeInterface();
    mShaderResourceInterface.SetReflectionSize();
    mShaderResourceInterface.SetReflection(nullptr);
}

void
ShaderProgram::BuildShaderResourceInterface(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mShaderResourceInterface.SetReflection(mShaderCompiler->GetShaderReflection());
    mShaderResourceInterface.CreateInterface();
    mShaderResourceInterface.SetReflection(nullptr);
    mShaderResourceInterface.AllocateUniformClientData();
    mShaderResourceInterface.AllocateUniformBufferObjects(mVkContext);

    mShaderResourceInterface.SetActiveUniformMaxLength();
    mShaderResourceInterface.SetActiveAttributeMaxLength();

    AllocateVkDescriptoSet();
    mUpdateDescriptorSets = true;
    mUpdateDescriptorData = true;
}
