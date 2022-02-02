#include "vulkan/vulkan_raii.hpp"

#include <iostream>

static std::string AppName = "App";
static std::string EngineName = "Engine";

int main(int argc, char **argv) {
  try {
    vk::raii::Context context;
    vk::ApplicationInfo applicationInfo(AppName.c_str(), 1, EngineName.c_str(),
                                        1, VK_API_VERSION_1_1);
    vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);
    vk::raii::Instance instance(context, instanceCreateInfo);
    vk::raii::PhysicalDevice physicalDevice =
        std::move(vk::raii::PhysicalDevices(instance).front());
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