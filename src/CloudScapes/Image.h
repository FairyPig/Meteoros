#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"
#include "BufferUtils.h"

namespace Image
{
	VkCommandBuffer beginSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool);
	void endSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool, VkCommandBuffer commandBuffer);

	void copyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void copyBufferToImage(VulkanDevice* device, VkCommandPool commandPool, VkBuffer buffer, VkImage& image, uint32_t width, uint32_t height);

	void transitionImageLayout(VulkanDevice* device, VkCommandPool commandPool, VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	void createImage(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	void createImageView(VulkanDevice* device, VkImageView& imageView, VkImage& textureImage,
		VkFormat format, VkImageAspectFlags aspectFlags);

	bool hasStencilComponent(VkFormat format);

	//load an image and upload it into a Vulkan image object
	void loadTexture(VulkanDevice* device, VkCommandPool& commandPool, const char* imagePath,
		VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

	void createSampler(VulkanDevice* device, VkSampler& sampler);
}