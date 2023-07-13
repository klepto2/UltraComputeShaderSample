#include "UltraEngine.h"
#include "ComputeShader.h"
#include "VulkanUtils.h"

using namespace std;
using namespace UltraEngine;
using namespace UltraEngine::Compute::Utils::initializers;
using namespace UltraEngine::Compute::Utils::tools;
using namespace UltraEngine::Compute::Utils;

namespace UltraEngine::Compute
{
	shared_ptr<ComputeDescriptorPool> ComputeShader::DescriptorPool = nullptr;

	void BeginComputeShaderDispatch(const UltraEngine::Render::VkRenderer& renderer, shared_ptr<Object> extra)
	{
		auto info = extra->As<ComputeDispatchInfo>();
		if (info != nullptr)
		{
			info->ComputeShader->Dispatch(renderer.commandbuffer, info->Tx, info->Ty, info->Tz, info->pushConstants, info->pushConstantsSize, info->pushConstantsOffset);
		}
	}

	VkImageView createImageView(VkDevice device, shared_ptr<Texture> texture) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture->GetImage();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = (VkFormat)texture->GetFormat();
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = texture->CountMipmaps();
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

		return imageView;
	}

	ComputeDescriptorPool::ComputeDescriptorPool(shared_ptr<World> world)
	{
		DescriptorPool = VK_NULL_HANDLE;
	};

	void ComputeShader::initLayout(VkDevice device)
	{
		for (int index = 0; index < _bufferData.size(); index++)
		{
			VkDescriptorSetLayoutBinding binding;
			binding.binding = _layoutBindings.size() + bufferoffset;
			binding.descriptorCount = 1;
			binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			binding.pImmutableSamplers = nullptr;

			if (_bufferData[index]->Texture != nullptr)
			{
				if (_bufferData[index]->IsWrite)
				{
					binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				}
				else
				{
					binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				}
			}

			if (_bufferData[index]->Data != nullptr)
			{
				if (_bufferData[index]->IsDynamic)
				{
					binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				}
				else
				{
					binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				}
			}

			_layoutBindings.push_back(binding);
			addtoPoolsize(binding.descriptorType);
		}

		VkDescriptorSetLayoutCreateInfo descriptorLayout = initializers::descriptorSetLayoutCreateInfo(_layoutBindings);

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &_computePipeLine->setLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&_computePipeLine->setLayout, 1);


		if (_constantData != nullptr)
		{
			//this push constant range starts at the beginning
			_constantDataRange.offset = 0;
			//this push constant range takes up the size of a MeshPushConstants struct
			_constantDataRange.size = _constantData->datasize;
			//this push constant range is accessible only in the vertex shader
			_constantDataRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			pPipelineLayoutCreateInfo.pPushConstantRanges = &_constantDataRange;
			pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		}

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &_computePipeLine->pipelineLayout));

		if (!ComputeShader::DescriptorPool->Initialized)
		{
			ComputeShader::DescriptorPool->Init(device);
		}

		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pPoolSizes = _poolSizes.data();
		descriptorPoolInfo.poolSizeCount = _poolSizes.size();
		descriptorPoolInfo.maxSets = 3;

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &_computePipeLine->descriptorPool));

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _computePipeLine->descriptorPool;
		allocInfo.pSetLayouts = &_computePipeLine->setLayout;
		allocInfo.descriptorSetCount = 1;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &_computePipeLine->descriptorSet));
	}

	void ComputeShader::initLayoutData(VkDevice device)
	{
		for (int index = 0; index < _bufferData.size(); index++)
		{
			if (_bufferData[index]->Texture != nullptr)
			{
				auto imageInfo = new VkDescriptorImageInfo;
				imageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				_bufferData[index]->createImageView(device, _bufferData[index]->Texture);
				imageInfo->imageView = _bufferData[index]->mipmapImage;
				imageInfo->sampler = _bufferData[index]->Texture->GetSampler();

				if (_bufferData[index]->IsWrite)
				{

					auto descrWrite = initializers::writeDescriptorSet(
						_computePipeLine->descriptorSet,
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						index,
						imageInfo
					);

					_writeDescriptorSets.push_back(descrWrite);
				}
				else
				{
					auto descrWrite = initializers::writeDescriptorSet(
						_computePipeLine->descriptorSet,
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						index,
						imageInfo
					);

					_writeDescriptorSets.push_back(descrWrite);
				}
			}

			if (_bufferData[index]->Data != nullptr)
			{
				auto gpudevice = UltraEngine::Core::GameEngine::Get()->renderingthreadmanager->device;

				VK_CHECK_RESULT(createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&_bufferData[index]->InternalBuffer,
					_bufferData[index]->datasize, _bufferData[index]->Data));

				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.dstSet = _computePipeLine->descriptorSet;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSet.dstBinding = index + bufferoffset;
				writeDescriptorSet.pBufferInfo = &_bufferData[index]->InternalBuffer.descriptor;
				writeDescriptorSet.descriptorCount = 1;
				_writeDescriptorSets.push_back(writeDescriptorSet);
			}
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(_writeDescriptorSets.size()), _writeDescriptorSets.data(), 0, NULL);
	}

	void ComputeShader::init(VkDevice device)
	{
		if (!_initialized)
		{
			_computePipeLine = make_shared<ComputePipeline>();
			
			initLayout(device);

			VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
			info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			info.stage.module = _shaderModule->GetHandle();
			info.stage.pName = "main";
			info.layout = _computePipeLine->pipelineLayout;

			VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &_computePipeLine->pipeline));

			initLayoutData(device);

			_initialized = true;
		}
	}

	void ComputeShader::updateData(VkDevice device)
	{
		bool updateDescriptorSet = false;

		for (int index = 0; index < _bufferData.size(); index++)
		{
			if (_bufferData[index]->Update)
			{
				if (_bufferData[index]->Data != nullptr)
				{
					_bufferData[index]->InternalBuffer.map();

					_bufferData[index]->InternalBuffer.copyTo(_bufferData[index]->Data, _bufferData[index]->datasize);

					_bufferData[index]->InternalBuffer.unmap();

					updateDescriptorSet = true;
				}

				if (_bufferData[index]->Texture != nullptr)
				{
					auto imageInfo = new VkDescriptorImageInfo;
					imageInfo->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;


					_bufferData[index]->createImageView(device, _bufferData[index]->Texture);
					imageInfo->imageView = _bufferData[index]->mipmapImage;
					imageInfo->sampler = _bufferData[index]->Texture->GetSampler();
					_writeDescriptorSets[index].pImageInfo = imageInfo;
					updateDescriptorSet = true;
				}

				_bufferData[index]->Update = false;
			}
		}

		if (updateDescriptorSet)
		{
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(_writeDescriptorSets.size()), _writeDescriptorSets.data(), 0, NULL);
		}
	}

	void ComputeShader::addtoPoolsize(VkDescriptorType descriptionType)
	{
		bool existed = false;
		for (int i = 0; i < _poolSizes.size(); i++)
		{
			if (_poolSizes[i].type == descriptionType)
			{
				_poolSizes[i].descriptorCount++;
				existed = true;
			}
		}

		if (!existed)
		{
			VkDescriptorPoolSize size;
			size.type = descriptionType;
			size.descriptorCount = 1;
			_poolSizes.push_back(size);
		}
	}

	ComputeShader::ComputeShader(shared_ptr<ShaderModule> module)
	{
		_shaderModule = module;
		_timestampQuery = make_shared<TimeStampQuery>();
	}

	shared_ptr<ComputeShader> ComputeShader::Create(const WString& path)
	{
		auto mod = LoadShaderModule(path);
		auto shader = make_shared<ComputeShader>(mod);
		return shader;
	}

	void ComputeShader::BeginDispatch(shared_ptr<World> world, int tx, int ty, int tz, bool oneTime, ComputeHook hook, void* pushData, size_t pushDataSize, int pushDataOffset)
	{
		auto info = make_shared<ComputeDispatchInfo>();
		info->ComputeShader = this->Self()->As<ComputeShader>();
		info->Tx = tx;
		info->Ty = ty;
		info->Tz = tz;
		info->World = world;
		info->Self = info;
		info->pushConstants = pushData;
		info->pushConstantsSize = pushDataSize;
		info->pushConstantsOffset = pushDataOffset;
		info->hook = hook;


		if (ComputeShader::DescriptorPool == nullptr)
		{
			ComputeShader::DescriptorPool = make_shared<ComputeDescriptorPool>(world);
		}
		switch (hook)
		{
		case ComputeHook::RENDER:
			world->AddHook(HookID::HOOKID_RENDER, BeginComputeShaderDispatch, info, !oneTime);
			break;
		case ComputeHook::TRANSFER:
			world->AddHook(HookID::HOOKID_TRANSFER, BeginComputeShaderDispatch, info, !oneTime);
			break;
		}
	}

	void ComputeShader::Dispatch(VkCommandBuffer cBuffer, int tx, int ty, int tz, void* pushData, size_t pushDataSize, int pushDataOffset)
	{
		auto manager = UltraEngine::Core::GameEngine::Get()->renderingthreadmanager;

		VkDevice device = manager->device->device;

		_timestampQuery->Init(manager->device->physicaldevice, manager->device->device);

		_timestampQuery->Reset(cBuffer);

		//just for testing

		vector<VkImageMemoryBarrier> barriers;

		bool barrierActive = false;
		bool isValid = true;
		auto path = _shaderModule->GetPath();

		for (int index = 0; index < _bufferData.size(); index++)
		{
			if (_bufferData[index]->Texture != nullptr && _bufferData[index]->IsWrite)
			{
				u_int layercount = 1;
				if (_bufferData[index]->Texture->GetType() == TEXTURE_CUBE)
				{
					layercount = 6;
				}

				tools::setImageLayout(cBuffer, _bufferData[index]->Texture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
					{ VK_IMAGE_ASPECT_COLOR_BIT, (u_int)_bufferData[index]->Texture->CountMipmaps() - 1,VK_REMAINING_MIP_LEVELS, 0, layercount }, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

				tools::insertImageMemoryBarrier(cBuffer, _bufferData[index]->Texture->GetImage(), 0, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					{ VK_IMAGE_ASPECT_COLOR_BIT, (u_int)_bufferData[index]->Texture->CountMipmaps() - 1,VK_REMAINING_MIP_LEVELS, 0, layercount });
				break;
			}

		}

		//initializes the layout and Writedescriptors
		init(device);

		//updates the uniform buffer data when needed
		updateData(device);

		vkCmdBindPipeline(cBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeLine->pipeline);

		// Bind descriptor set.
		vkCmdBindDescriptorSets(cBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeLine->pipelineLayout, 0, 1,
			&_computePipeLine->descriptorSet, 0, nullptr);

		// Bind the compute pipeline.
		if (pushData != nullptr)
		{
			vkCmdPushConstants(cBuffer, _computePipeLine->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, pushDataOffset, pushDataSize, pushData);
		}

		_timestampQuery->write(cBuffer, 0);
		// Dispatch compute job.
		vkCmdDispatch(cBuffer, tx, ty, tz);

		_timestampQuery->write(cBuffer, 1);

		if (barrierActive)
		{
			for (int i = 0; i < barriers.size(); i++)
			{
				// Release barrier from compute queue
				barriers[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barriers[i].dstAccessMask = 0;
				barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				vkCmdPipelineBarrier(
					cBuffer,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barriers[i]);
			}
		}
	}

	int ComputeShader::AddTargetImage(shared_ptr<Texture> texture, int miplevel)
	{
		auto data = make_shared<ComputeBufferData>();
		data->Texture = texture;
		data->mipLevel = miplevel;
		data->IsWrite = true;
		data->IsDynamic = false;

		_bufferData.push_back(data);
		return _bufferData.size() - 1;
	}

	int ComputeShader::AddSampler(shared_ptr<Texture> texture)
	{
		auto data = make_shared<ComputeBufferData>();
		data->Texture = texture;
		data->IsWrite = false;
		data->IsDynamic = false;

		_bufferData.push_back(data);
		return _bufferData.size() - 1;
	}

	int ComputeShader::AddUniformBuffer(void* data, size_t dataSize, bool dynamic)
	{
		auto ubodata = make_shared<ComputeBufferData>();
		ubodata->Data = data;
		ubodata->datasize = dataSize;
		ubodata->IsWrite = false;
		ubodata->IsDynamic = dynamic;

		_bufferData.push_back(ubodata);
		return _bufferData.size() - 1;
	}

	void ComputeShader::SetupPushConstant(size_t dataSize)
	{
		auto ubodata = make_shared<ComputeBufferData>();
		ubodata->Data = nullptr;
		ubodata->datasize = dataSize;
		ubodata->IsPushConstant = true;
		_constantData = ubodata;
	}

	void ComputeShader::Update(int layoutIndex)
	{
		_bufferData[layoutIndex]->Update = true;
	}

	void ComputeShader::UpdateTexture(int layoutIndex, shared_ptr<Texture> texture)
	{
		_bufferData[layoutIndex]->Texture = texture;
		_bufferData[layoutIndex]->Update = true;
	}

	void ComputeBufferData::createImageView(VkDevice device, shared_ptr<UltraEngine::Texture> texture)
	{
		int faces = 1;
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture->GetImage();
		switch (texture->GetType())
		{
			//case TEXTURE_1D:
			//	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
			//	break;
		case TEXTURE_2D:
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			break;
		case TEXTURE_3D:
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		case TEXTURE_CUBE:
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			faces = texture->CountFaces();
			break;
		}

		viewInfo.format = VkFormat(texture->GetFormat());
		switch (viewInfo.format)
		{
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D16_UNORM_S8_UINT:
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
		default:
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		viewInfo.subresourceRange.baseMipLevel = mipLevel;
		viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = faces;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, NULL, &mipmapImage));
	}
}