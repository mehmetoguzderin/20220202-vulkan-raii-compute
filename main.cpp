#include <vulkan/vulkan_raii.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

vk::raii::DeviceMemory
vkMalloc(vk::raii::Device const &device,
         vk::PhysicalDeviceMemoryProperties const &memoryProperties,
         vk::MemoryRequirements const &memoryRequirements,
         vk::MemoryPropertyFlags memoryPropertyFlags) {
  uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;
  uint32_t memoryTypeIndex = uint32_t(~0);
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
    if ((memoryTypeBits & 1) &&
        ((memoryProperties.memoryTypes[i].propertyFlags &
          memoryPropertyFlags) == memoryPropertyFlags)) {
      memoryTypeIndex = i;
      break;
    }
    memoryTypeBits >>= 1;
  }
  assert(memoryTypeIndex != uint32_t(~0));
  vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size,
                                            memoryTypeIndex);
  return vk::raii::DeviceMemory(device, memoryAllocateInfo);
}

std::vector<uint32_t> vkReadSPV(std::string const &filename) {
  std::vector<uint32_t> data;
  std::ifstream file;
  file.open(filename, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }
  file.seekg(0, std::ios::end);
  uint64_t read_count = static_cast<uint64_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  data.resize(static_cast<size_t>(read_count / sizeof(uint32_t)));
  file.read(reinterpret_cast<char *>(data.data()), read_count);
  file.close();
  return data;
}

static std::string AppName = "App";
static std::string EngineName = "Engine";

int main(int argc, char **argv) {
  int32_t localSize = 8;
  size_t memorySize = localSize * localSize * sizeof(float);
  try {
    vk::raii::Context context;
    vk::ApplicationInfo applicationInfo(AppName.c_str(), 1, EngineName.c_str(),
                                        1, VK_API_VERSION_1_1);
    vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);
    vk::raii::Instance instance(context, instanceCreateInfo);
    vk::raii::PhysicalDevice physicalDevice =
        std::move(vk::raii::PhysicalDevices(instance).front());
    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, 0, 1, &queuePriority);
    vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo);
    vk::raii::Device device(physicalDevice, deviceCreateInfo);
    vk::CommandPoolCreateInfo commandPoolCreateInfo({}, 0);
    vk::raii::CommandPool commandPool(device, commandPoolCreateInfo);
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
        *commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::raii::CommandBuffer commandBuffer = std::move(
        vk::raii::CommandBuffers(device, commandBufferAllocateInfo).front());
    vk::raii::Queue queue(device, 0, 0);
    vk::BufferCreateInfo bufferCreateInfo(
        {}, memorySize, vk::BufferUsageFlagBits::eStorageBuffer);
    vk::raii::Buffer deviceBuffer(device, bufferCreateInfo);
    vk::raii::DeviceMemory deviceMemory =
        vkMalloc(device, physicalDevice.getMemoryProperties(),
                 deviceBuffer.getMemoryRequirements(),
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent |
                     vk::MemoryPropertyFlagBits::eDeviceLocal);
    deviceBuffer.bindMemory(*deviceMemory, 0);
    vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(
        0, vk::DescriptorType::eStorageBuffer, 1,
        vk::ShaderStageFlagBits::eCompute);
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
        {}, descriptorSetLayoutBinding);
    vk::raii::DescriptorSetLayout descriptorSetLayout(
        device, descriptorSetLayoutCreateInfo);
    vk::DescriptorPoolSize poolSize(vk::DescriptorType::eStorageBuffer, 1);
    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSize);
    vk::raii::DescriptorPool descriptorPool(device, descriptorPoolCreateInfo);
    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
        *descriptorPool, *descriptorSetLayout);
    vk::raii::DescriptorSet descriptorSet = std::move(
        vk::raii::DescriptorSets(device, descriptorSetAllocateInfo).front());
    vk::DescriptorBufferInfo descriptorBufferInfo(*deviceBuffer, 0, memorySize);
    vk::WriteDescriptorSet writeDescriptorSet(
        *descriptorSet, 0, 0, vk::DescriptorType::eStorageBuffer, {},
        descriptorBufferInfo);
    device.updateDescriptorSets(writeDescriptorSet, nullptr);
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo({},
                                                          *descriptorSetLayout);
    vk::raii::PipelineLayout pipelineLayout(device, pipelineLayoutCreateInfo);
    std::vector<uint32_t> computeShaderSPV = vkReadSPV("main.comp.spv");
    vk::ShaderModuleCreateInfo computeShaderModuleCreateInfo({},
                                                             computeShaderSPV);
    vk::raii::ShaderModule computeShaderModule(device,
                                               computeShaderModuleCreateInfo);
    vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo =
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute,
                                          *computeShaderModule, "main");
    vk::ComputePipelineCreateInfo computePipelineCreateInfo(
        {}, pipelineShaderStageCreateInfo, *pipelineLayout);
    vk::raii::Pipeline pipeline(device, nullptr, computePipelineCreateInfo);
    commandBuffer.begin({});
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                     *pipelineLayout, 0, {*descriptorSet},
                                     nullptr);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
    commandBuffer.dispatch(localSize, 1, 1);
    commandBuffer.end();
    vk::SubmitInfo submitInfo({}, {}, *commandBuffer);
    queue.submit(submitInfo);
    device.waitIdle();
    float *pData = static_cast<float *>(deviceMemory.mapMemory(0, memorySize));
    for (uint32_t i = 0; i < localSize * localSize; ++i) {
      std::cout << i << ": " << pData[i] << "\n";
    }
    deviceMemory.unmapMemory();
    std::cout << physicalDevice.getProperties().deviceName << "\n";
  } catch (vk::SystemError &err) {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    exit(-1);
  } catch (std::exception &err) {
    std::cout << "std::exception: " << err.what() << std::endl;
    exit(-1);
  } catch (...) {
    std::cout << "unknown error\n";
    exit(-1);
  }

  return 0;
}
