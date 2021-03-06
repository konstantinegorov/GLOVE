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
 *  @file       resourceManager.cpp
 *  @author     Think Silicon
 *  @date       25/07/2018
 *  @version    1.0
 *
 *  @brief      Resource Manager Functionality in GLOVE
 *
 *  @section
 *
 *  OpenGL ES allows developers to allocate, edit and delete a variety of
 *  resources. These include Generic Vertex Attributes, Buffers, Renderbuffers,
 *  Framebuffers, Textures, Shaders, and Shader Programs.
 */

#include "resourceManager.h"

ResourceManager::ResourceManager(const vulkanAPI::vkContext_t *vkContext):
    mVkContext(vkContext),
    mShadingObjectCount(1),
    mGenericVertexAttributes(GLOVE_MAX_VERTEX_ATTRIBS)
{
    FUN_ENTRY(GL_LOG_TRACE);

    CreateDefaultTextures();

    for(auto& gva : mGenericVertexAttributes) {
        gva.SetVkContext(vkContext);
    }
}

ResourceManager::~ResourceManager()
{
    FUN_ENTRY(GL_LOG_TRACE);

    delete mDefaultTexture2D;
    delete mDefaultTextureCubeMap;

    for(auto& gva : mGenericVertexAttributes) {
        gva.Release();
    }
    mGenericVertexAttributes.clear();
}

void
ResourceManager::SetCacheManager(CacheManager *cacheManager)
{
    for(auto& gva : mGenericVertexAttributes) {
        gva.SetCacheManager(cacheManager);
    }
}

void
ResourceManager::CreateDefaultTextures()
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mDefaultTexture2D = new Texture(mVkContext);
    mDefaultTexture2D->SetTarget(GL_TEXTURE_2D);
    mDefaultTexture2D->SetVkFormat(VK_FORMAT_R8G8B8A8_UNORM);
    mDefaultTexture2D->SetVkImageUsage(static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    mDefaultTexture2D->SetVkImageTarget(vulkanAPI::Image::VK_IMAGE_TARGET_2D);
    mDefaultTexture2D->SetVkImageTiling();
    mDefaultTexture2D->InitState();

    mDefaultTextureCubeMap = new Texture(mVkContext);
    mDefaultTextureCubeMap->SetTarget(GL_TEXTURE_CUBE_MAP);
    mDefaultTextureCubeMap->SetVkFormat(VK_FORMAT_R8G8B8A8_UNORM);
    mDefaultTextureCubeMap->SetVkImageUsage(static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    mDefaultTextureCubeMap->SetVkImageTarget(vulkanAPI::Image::VK_IMAGE_TARGET_CUBE);
    mDefaultTextureCubeMap->SetVkImageTiling();
    mDefaultTextureCubeMap->InitState();
}


uint32_t
ResourceManager::PushShadingObject(const ShadingNamespace_t& obj)
{
    FUN_ENTRY(GL_LOG_TRACE);
    mShadingObjectPool[mShadingObjectCount] = obj;
    return mShadingObjectCount++;
}

void
ResourceManager::EraseShadingObject(uint32_t id)
{
    FUN_ENTRY(GL_LOG_TRACE);
    mShadingObjectPool.erase(id);
}

GLboolean
ResourceManager::IsShadingObject(GLuint index, shadingNamespaceType_t type) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(!index || index >= mShadingObjectCount || !ShadingObjectExists(index)) {
        return GL_FALSE;
    }

    ShadingNamespace_t shadId = mShadingObjectPool.find(index)->second;
    return (shadId.arrayIndex && shadId.type == type) ? GL_TRUE : GL_FALSE;
}

uint32_t
ResourceManager::FindShaderID(const Shader *shader)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    for(shadingPoolIDs_t::iterator it = mShadingObjectPool.begin(); it != mShadingObjectPool.end(); ++it) {
        if(it->second.type == SHADER_ID && GetShaderID(shader) == it->second.arrayIndex) {
            return it->first;
        }
    }
    return 0;
}

uint32_t
ResourceManager::FindShaderProgramID(const ShaderProgram *program)
{
   FUN_ENTRY(GL_LOG_DEBUG);

   for(shadingPoolIDs_t::iterator it = mShadingObjectPool.begin(); it != mShadingObjectPool.end(); ++it) {
        if(it->second.type == SHADER_PROGRAM_ID && GetShaderProgramID(program) == it->second.arrayIndex) {
            return it->first;
        }
    }
    return 0;
}


void
ResourceManager::UpdateFramebufferObjects(GLuint index, GLenum target)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    for(typename map<uint32_t, Framebuffer *>::const_iterator it =
        mFramebuffers.GetObjects()->begin(); it != mFramebuffers.GetObjects()->end(); it++) {

        Framebuffer *fb = it->second;
        if((fb->GetColorAttachmentType()   == target && index == fb->GetColorAttachmentName()) ||
           (fb->GetDepthAttachmentType()   == target && index == fb->GetDepthAttachmentName()) ||
           (fb->GetStencilAttachmentType() == target && index == fb->GetStencilAttachmentName())) {
            fb->SetUpdated();
        }
    }
}

bool
ResourceManager::IsTextureAttachedToFBO(const Texture *texture)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    for(typename map<uint32_t, Framebuffer *>::const_iterator it =
        mFramebuffers.GetObjects()->begin(); it != mFramebuffers.GetObjects()->end(); it++) {

        if(it->second->GetColorAttachmentType() == GL_TEXTURE && texture == it->second->GetColorAttachmentTexture()) {
            return true;
        }
    }

    return false;
}

void
ResourceManager::CleanPurgeList()
{
    FUN_ENTRY(GL_LOG_DEBUG);

    //Buffers
    for (auto it = mPurgeListBufferObject.begin(); it != mPurgeListBufferObject.end(); ) {
        if ((*it)->GetRefCount() == 0) {
            delete *it;
            it = mPurgeListBufferObject.erase(it);
        } else {
            ++it;
        }
    }
    //Textures
    for (auto it = mPurgeListTexture.begin(); it != mPurgeListTexture.end(); ) {
        if ((*it)->GetRefCount() == 0) {
            delete *it;
            it = mPurgeListTexture.erase(it);
        } else {
            ++it;
        }
    }
    //Shader Programs
    for (auto it = mPurgeListShaderPrograms.begin(); it != mPurgeListShaderPrograms.end();) {
        ShaderProgram* shaderProgramPtr = *it;
        if (shaderProgramPtr->FreeForDeletion()) {
            shaderProgramPtr->DetachShaders();
            uint32_t id = FindShaderProgramID(shaderProgramPtr);
            EraseShadingObject(id);
            DeallocateShaderProgram(shaderProgramPtr);
            it = mPurgeListShaderPrograms.erase(it);
        } else {
            ++it;
        }
    }
    //Shaders
    for (auto it = mPurgeListShaders.begin(); it != mPurgeListShaders.end();) {
        Shader* shaderPtr = *it;
        if (shaderPtr->FreeForDeletion()) {
            uint32_t id = FindShaderID(shaderPtr);
            EraseShadingObject(id);
            DeallocateShader(shaderPtr);
            it = mPurgeListShaders.erase(it);
        } else {
            ++it;
        }
    }
    //Renderbuffer
    for (auto it = mPurgeListRenderbuffers.begin(); it != mPurgeListRenderbuffers.end(); ) {
        if ((*it)->GetRefCount() == 0) {
            delete *it;
            it = mPurgeListRenderbuffers.erase(it);
        } else {
            ++it;
        }
    }
}

void
ResourceManager::FramebufferCacheAttachement(Texture *texture, GLuint index)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    for(typename map<uint32_t, Framebuffer *>::const_iterator it =
        mFramebuffers.GetObjects()->begin(); it != mFramebuffers.GetObjects()->end(); it++) {

        it->second->CacheAttachement(texture, index);
    }
}

void
ResourceManager::FramebufferCacheAttachement(Renderbuffer *renderbuffer, GLuint index)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    for(typename map<uint32_t, Framebuffer *>::const_iterator it =
        mFramebuffers.GetObjects()->begin(); it != mFramebuffers.GetObjects()->end(); it++) {

        it->second->CacheAttachement(renderbuffer, index);
    }
}
