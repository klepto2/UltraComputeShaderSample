#pragma once
#include "VulkanUtils.h"
using namespace UltraEngine::Compute::Utils;


namespace UltraEngine::Compute
{
	class ComputeShader;
	class ComputePipelineBuilder;
	class ComputeDispatchInfo;

	void BeginComputeShaderDispatch(const UltraEngine::Render::VkRenderer& renderer, shared_ptr<Object> extra);

	enum class ComputeHook
	{
		RENDER = HOOKID_RENDER,
		TRANSFER = HOOKID_TRANSFER
	};

	struct ComputePipeline
	{
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	};

	class BuilderCommandBase : Object
	{
	public:
		virtual VkWriteDescriptorSet Execute(shared_ptr<ComputePipelineBuilder> builder, ComputePipeline& pipeline, vector<VkWriteDescriptorSet>& writeDescriportSets)
		{
			return VkWriteDescriptorSet{};
		};
	};

	class ComputeDispatchInfo : public Object
	{
	public:
		shared_ptr<ComputeDispatchInfo> Self;
		shared_ptr<ComputeShader> ComputeShader;
		int Tx;
		int Ty;
		int Tz;
		shared_ptr<World> World;
		void* pushConstants = nullptr;
		size_t pushConstantsSize = 0;
		int pushConstantsOffset = 0;
		ComputeHook hook = ComputeHook::RENDER;
		int callCount = 0;
	};

	class ComputeBufferData : Object
	{
	public:
		void* Data = nullptr;
		size_t datasize;
		shared_ptr<Texture> Texture = nullptr;
		int mipLevel = 0;
		ComputeBuffer InternalBuffer;
		bool IsDynamic = false;
		bool IsWrite = false;
		bool Update = false;
		bool IsPushConstant = false;
		VkImageView mipmapImage = VK_NULL_HANDLE;

		void createImageView(VkDevice device, shared_ptr<UltraEngine::Texture> texture);
	};

	class ComputeShader : public Object
	{
	private:
		shared_ptr<ShaderModule> _shaderModule;
		std::vector<VkDescriptorSetLayoutBinding> _layoutBindings;
		std::vector<VkDescriptorPoolSize> _poolSizes;
		std::vector<VkWriteDescriptorSet> _writeDescriptorSets;
		shared_ptr<TimeStampQuery> _timestampQuery;

		vector<shared_ptr<ComputeBufferData>> _bufferData;
		shared_ptr<ComputeBufferData> _constantData;
		VkPushConstantRange _constantDataRange;

		VkPipelineCache _pipelineCache;
		shared_ptr<ComputePipeline> _computePipeLine;

		bool _executed = false;
		bool _initialized = false;
		void initLayout(VkDevice device);
		void initLayoutData(VkDevice device);
		void init(VkDevice device);
		void updateData(VkDevice device);
		void addtoPoolsize(VkDescriptorType descriptionType);


	public:
		static shared_ptr<ComputeDescriptorPool> DescriptorPool;
		ComputeShader(shared_ptr<ShaderModule> module);
		static shared_ptr<ComputeShader> Create(const WString& path);
		int bufferoffset = 0;
		void BeginDispatch(shared_ptr<World> world, int tx, int ty, int tz, bool oneTime = true, ComputeHook hook = ComputeHook::RENDER, void* pushData = nullptr, size_t pushDataSize = 0, int pushDataOffset = 0);
		void Dispatch(VkCommandBuffer cBuffer, int tx, int ty, int tz, void* pushData, size_t pushDataSize, int pushDataOffset);
		int AddTargetImage(shared_ptr<Texture> texture, int miplevel = 0);
		int AddSampler(shared_ptr<Texture> texture);
		int AddUniformBuffer(void* data, size_t dataSize, bool dynamic);
		void SetupPushConstant(size_t dataSize);
		void Update(int layoutIndex = 0);
		void UpdateTexture(int layoutIndex, shared_ptr<Texture> texture);
		shared_ptr<TimeStampQuery> GetQueryTimer() { return _timestampQuery; };
	};

}



