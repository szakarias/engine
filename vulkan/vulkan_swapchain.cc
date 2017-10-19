// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/vulkan/vulkan_swapchain.h"

#include "flutter/vulkan/vulkan_backbuffer.h"
#include "flutter/vulkan/vulkan_device.h"
#include "flutter/vulkan/vulkan_image.h"
#include "flutter/vulkan/vulkan_proc_table.h"
#include "flutter/vulkan/vulkan_surface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "third_party/skia/src/gpu/vk/GrVkUtil.h"

namespace vulkan {

VulkanSwapchain::VulkanSwapchain(const VulkanProcTable& p_vk,
                                 const VulkanDevice& device,
                                 const VulkanSurface& surface,
                                 GrContext* skia_context,
                                 std::unique_ptr<VulkanSwapchain> old_swapchain,
                                 uint32_t queue_family_index)
    : vk(p_vk),
      device_(device),
      capabilities_(),
      surface_format_(),
      current_pipeline_stage_(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
      current_backbuffer_index_(0),
      current_image_index_(0),
      valid_(false) {
  if (!device_.IsValid() || !surface.IsValid() || skia_context == nullptr) {
    FXL_DLOG(INFO) << "Device or surface is invalid.";
    return;
  }

  if (!device_.GetSurfaceCapabilities(surface, &capabilities_)) {
    FXL_DLOG(INFO) << "Could not find surface capabilities.";
    return;
  }

  if (!device_.ChooseSurfaceFormat(surface, &surface_format_)) {
    FXL_DLOG(INFO) << "Could not choose surface format.";
    return;
  }

  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
  if (!device_.ChoosePresentMode(surface, &present_mode)) {
    FXL_DLOG(INFO) << "Could not choose present mode.";
    return;
  }

  // Check if the surface can present.

  VkBool32 supported = VK_FALSE;
  if (VK_CALL_LOG_ERROR(vk.GetPhysicalDeviceSurfaceSupportKHR(
          device_.GetPhysicalDeviceHandle(),  // physical device
          queue_family_index,                 // queue family
          surface.Handle(),                   // surface to test
          &supported)) != VK_SUCCESS) {
    FXL_DLOG(INFO) << "Could not get physical device surface support.";
    return;
  }

  if (supported != VK_TRUE) {
    FXL_DLOG(INFO) << "Surface was not supported by the physical device.";
    return;
  }

  // Construct the Swapchain

  VkSwapchainKHR old_swapchain_handle = VK_NULL_HANDLE;

  if (old_swapchain != nullptr && old_swapchain->IsValid()) {
    old_swapchain_handle = old_swapchain->swapchain_;
    // The unique pointer to the swapchain will go out of scope here
    // and its handle collected after the appropriate device wait.
  }

  VkSurfaceKHR surface_handle = surface.Handle();

  const VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface_handle,
      .minImageCount = capabilities_.minImageCount,
      .imageFormat = surface_format_.format,
      .imageColorSpace = surface_format_.colorSpace,
      .imageExtent = capabilities_.currentExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,  // Because of the exclusive sharing mode.
      .pQueueFamilyIndices = nullptr,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_FALSE,
      .oldSwapchain = old_swapchain_handle,
  };

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;

  if (VK_CALL_LOG_ERROR(vk.CreateSwapchainKHR(device_.GetHandle(), &create_info,
                                              nullptr, &swapchain)) !=
      VK_SUCCESS) {
    FXL_DLOG(INFO) << "Could not create the swapchain.";
    return;
  }

  swapchain_ = {swapchain, [this](VkSwapchainKHR swapchain) {
                  FXL_ALLOW_UNUSED_LOCAL(device_.WaitIdle());
                  vk.DestroySwapchainKHR(device_.GetHandle(), swapchain,
                                         nullptr);
                }};

  if (!CreateSwapchainImages(skia_context)) {
    FXL_DLOG(INFO) << "Could not create swapchain images.";
    return;
  }

  valid_ = true;
}

VulkanSwapchain::~VulkanSwapchain() = default;

bool VulkanSwapchain::IsValid() const {
  return valid_;
}

std::vector<VkImage> VulkanSwapchain::GetImages() const {
  uint32_t count = 0;
  if (VK_CALL_LOG_ERROR(vk.GetSwapchainImagesKHR(
          device_.GetHandle(), swapchain_, &count, nullptr)) != VK_SUCCESS) {
    return {};
  }

  if (count == 0) {
    return {};
  }

  std::vector<VkImage> images;

  images.resize(count);

  if (VK_CALL_LOG_ERROR(vk.GetSwapchainImagesKHR(
          device_.GetHandle(), swapchain_, &count, images.data())) !=
      VK_SUCCESS) {
    return {};
  }

  return images;
}

SkISize VulkanSwapchain::GetSize() const {
  VkExtent2D extents = capabilities_.currentExtent;

  if (extents.width < capabilities_.minImageExtent.width) {
    extents.width = capabilities_.minImageExtent.width;
  } else if (extents.width > capabilities_.maxImageExtent.width) {
    extents.width = capabilities_.maxImageExtent.width;
  }

  if (extents.height < capabilities_.minImageExtent.height) {
    extents.height = capabilities_.minImageExtent.height;
  } else if (extents.height > capabilities_.maxImageExtent.height) {
    extents.height = capabilities_.maxImageExtent.height;
  }

  return SkISize::Make(extents.width, extents.height);
}

static sk_sp<SkColorSpace> SkColorSpaceFromVkFormat(VkFormat format) {
  if (GrVkFormatIsSRGB(format, nullptr /* dont care */)) {
    return SkColorSpace::MakeSRGB();
  }

  if (format == VK_FORMAT_R16G16B16A16_SFLOAT) {
    return SkColorSpace::MakeSRGBLinear();
  }

  return nullptr;
}

sk_sp<SkSurface> VulkanSwapchain::CreateSkiaSurface(GrContext* gr_context,
                                                    VkImage image,
                                                    const SkISize& size) const {
  if (gr_context == nullptr) {
    return nullptr;
  }

  GrPixelConfig pixel_config = GrVkFormatToPixelConfig(surface_format_.format);

  if (pixel_config == kUnknown_GrPixelConfig) {
    // Vulkan format unsupported by Skia.
    return nullptr;
  }

  const GrVkImageInfo image_info = {
      .fImage = image,
      .fAlloc = {VK_NULL_HANDLE, 0, 0, 0},
      .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
      .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .fFormat = surface_format_.format,
      .fLevelCount = 1,
  };

  // TODO(chinmaygarde): Setup the stencil buffer and the sampleCnt.
  GrBackendRenderTarget backend_render_target(size.fWidth, size.fHeight, 0, 0,
                                              image_info);
  SkSurfaceProps props(SkSurfaceProps::InitType::kLegacyFontHost_InitType);

  return SkSurface::MakeFromBackendRenderTarget(
      gr_context,             // context
      backend_render_target,  // backend render target
      kTopLeft_GrSurfaceOrigin,
      SkColorSpaceFromVkFormat(surface_format_.format),  // colorspace
      &props                                             // surface properties
  );
}

bool VulkanSwapchain::CreateSwapchainImages(GrContext* skia_context) {
  std::vector<VkImage> images = GetImages();

  if (images.size() == 0) {
    return false;
  }

  const SkISize surface_size = GetSize();

  for (const VkImage& image : images) {
    // Populate the backbuffer.
    auto backbuffer = std::make_unique<VulkanBackbuffer>(
        vk, device_.GetHandle(), device_.GetCommandPool());

    if (!backbuffer->IsValid()) {
      return false;
    }

    backbuffers_.emplace_back(std::move(backbuffer));

    // Populate the image.
    auto vulkan_image = std::make_unique<VulkanImage>(image);

    if (!vulkan_image->IsValid()) {
      return false;
    }

    images_.emplace_back(std::move(vulkan_image));

    // Populate the surface.
    auto surface = CreateSkiaSurface(skia_context, image, surface_size);

    if (surface == nullptr) {
      return false;
    }

    surfaces_.emplace_back(std::move(surface));
  }

  FXL_DCHECK(backbuffers_.size() == images_.size());
  FXL_DCHECK(images_.size() == surfaces_.size());

  return true;
}

VulkanBackbuffer* VulkanSwapchain::GetNextBackbuffer() {
  auto available_backbuffers = backbuffers_.size();

  if (available_backbuffers == 0) {
    return nullptr;
  }

  auto next_backbuffer_index =
      (current_backbuffer_index_ + 1) % backbuffers_.size();

  auto& backbuffer = backbuffers_[next_backbuffer_index];

  if (!backbuffer->IsValid()) {
    return nullptr;
  }

  current_backbuffer_index_ = next_backbuffer_index;
  return backbuffer.get();
}

VulkanSwapchain::AcquireResult VulkanSwapchain::AcquireSurface() {
  AcquireResult error = {AcquireStatus::ErrorSurfaceLost, nullptr};

  if (!IsValid()) {
    FXL_DLOG(INFO) << "Swapchain was invalid.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 0:
  // Acquire the next available backbuffer.
  // ---------------------------------------------------------------------------
  auto backbuffer = GetNextBackbuffer();

  if (backbuffer == nullptr) {
    FXL_DLOG(INFO) << "Could not get the next backbuffer.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 1:
  // Wait for use readiness.
  // ---------------------------------------------------------------------------
  if (!backbuffer->WaitFences()) {
    FXL_DLOG(INFO) << "Failed waiting on fences.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 2:
  // Put semaphores in unsignaled state.
  // ---------------------------------------------------------------------------
  if (!backbuffer->ResetFences()) {
    FXL_DLOG(INFO) << "Could not reset fences.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 3:
  // Acquire the next image index.
  // ---------------------------------------------------------------------------
  uint32_t next_image_index = 0;

  VkResult acquire_result = VK_CALL_LOG_ERROR(
      vk.AcquireNextImageKHR(device_.GetHandle(),                   //
                             swapchain_,                            //
                             std::numeric_limits<uint64_t>::max(),  //
                             backbuffer->GetUsageSemaphore(),       //
                             VK_NULL_HANDLE,                        //
                             &next_image_index));

  switch (acquire_result) {
    case VK_SUCCESS:
      break;
    case VK_ERROR_OUT_OF_DATE_KHR:
      return {AcquireStatus::ErrorSurfaceOutOfDate, nullptr};
    case VK_ERROR_SURFACE_LOST_KHR:
      return {AcquireStatus::ErrorSurfaceLost, nullptr};
    default:
      FXL_LOG(INFO) << "Unexpected result from AcquireNextImageKHR: "
                    << acquire_result;
      return {AcquireStatus::ErrorSurfaceLost, nullptr};
  }

  // Simple sanity checking of image index.
  if (next_image_index >= images_.size()) {
    FXL_DLOG(INFO) << "Image index returned was out-of-bounds.";
    return error;
  }

  auto& image = images_[next_image_index];
  if (!image->IsValid()) {
    FXL_DLOG(INFO) << "Image at index was invalid.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 4:
  // Start recording to the command buffer.
  // ---------------------------------------------------------------------------
  if (!backbuffer->GetUsageCommandBuffer().Begin()) {
    FXL_DLOG(INFO) << "Could not begin recording to the command buffer.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 5:
  // Set image layout to color attachment mode.
  // ---------------------------------------------------------------------------
  VkPipelineStageFlagBits destination_pipeline_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkImageLayout destination_image_layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  if (!image->InsertImageMemoryBarrier(
          backbuffer->GetUsageCommandBuffer(),   // command buffer
          current_pipeline_stage_,               // src_pipeline_bits
          destination_pipeline_stage,            // dest_pipeline_bits
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // dest_access_flags
          destination_image_layout               // dest_layout
          )) {
    FXL_DLOG(INFO) << "Could not insert image memory barrier.";
    return error;
  } else {
    current_pipeline_stage_ = destination_pipeline_stage;
  }

  // ---------------------------------------------------------------------------
  // Step 6:
  // End recording to the command buffer.
  // ---------------------------------------------------------------------------
  if (!backbuffer->GetUsageCommandBuffer().End()) {
    FXL_DLOG(INFO) << "Could not end recording to the command buffer.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 7:
  // Submit the command buffer to the device queue.
  // ---------------------------------------------------------------------------
  std::vector<VkSemaphore> wait_semaphores = {backbuffer->GetUsageSemaphore()};
  std::vector<VkSemaphore> signal_semaphores = {};
  std::vector<VkCommandBuffer> command_buffers = {
      backbuffer->GetUsageCommandBuffer().Handle()};

  if (!device_.QueueSubmit(
          {destination_pipeline_stage},  // wait_dest_pipeline_stages
          wait_semaphores,               // wait_semaphores
          signal_semaphores,             // signal_semaphores
          command_buffers,               // command_buffers
          backbuffer->GetUsageFence()    // fence
          )) {
    FXL_DLOG(INFO) << "Could not submit to the device queue.";
    return error;
  }

  // ---------------------------------------------------------------------------
  // Step 8:
  // Tell Skia about the updated image layout.
  // ---------------------------------------------------------------------------
  sk_sp<SkSurface> surface = surfaces_[next_image_index];

  if (surface == nullptr) {
    FXL_DLOG(INFO) << "Could not access surface at the image index.";
    return error;
  }

  GrVkImageInfo* image_info = nullptr;
  if (!surface->getRenderTargetHandle(
          reinterpret_cast<GrBackendObject*>(&image_info),
          SkSurface::kFlushRead_BackendHandleAccess)) {
    FXL_DLOG(INFO) << "Could not get render target handle.";
    return error;
  }

  image_info->updateImageLayout(destination_image_layout);
  current_image_index_ = next_image_index;

  return {AcquireStatus::Success, surface};
}

bool VulkanSwapchain::Submit() {
  if (!IsValid()) {
    FXL_DLOG(INFO) << "Swapchain was invalid.";
    return false;
  }

  sk_sp<SkSurface> surface = surfaces_[current_image_index_];
  const std::unique_ptr<VulkanImage>& image = images_[current_image_index_];
  auto backbuffer = backbuffers_[current_backbuffer_index_].get();

  // ---------------------------------------------------------------------------
  // Step 0:
  // Notify to Skia that we will read from its backend object.
  // ---------------------------------------------------------------------------
  GrVkImageInfo* image_info = nullptr;
  if (!surface->getRenderTargetHandle(
          reinterpret_cast<GrBackendObject*>(&image_info),
          SkSurface::kFlushRead_BackendHandleAccess)) {
    FXL_DLOG(INFO) << "Could not get render target handle.";
    return false;
  }

  // ---------------------------------------------------------------------------
  // Step 1:
  // Start recording to the command buffer.
  // ---------------------------------------------------------------------------
  if (!backbuffer->GetRenderCommandBuffer().Begin()) {
    FXL_DLOG(INFO) << "Could not start recording to the command buffer.";
    return false;
  }

  // ---------------------------------------------------------------------------
  // Step 2:
  // Set image layout to present mode.
  // ---------------------------------------------------------------------------
  VkPipelineStageFlagBits destination_pipeline_stage =
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkImageLayout destination_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  if (!image->InsertImageMemoryBarrier(
          backbuffer->GetRenderCommandBuffer(),  // command buffer
          current_pipeline_stage_,               // src_pipeline_bits
          destination_pipeline_stage,            // dest_pipeline_bits
          VK_ACCESS_MEMORY_READ_BIT,             // dest_access_flags
          destination_image_layout               // dest_layout
          )) {
    FXL_DLOG(INFO) << "Could not insert memory barrier.";
    return false;
  } else {
    current_pipeline_stage_ = destination_pipeline_stage;
  }

  // ---------------------------------------------------------------------------
  // Step 3:
  // End recording to the command buffer.
  // ---------------------------------------------------------------------------
  if (!backbuffer->GetRenderCommandBuffer().End()) {
    FXL_DLOG(INFO) << "Could not end recording to the command buffer.";
    return false;
  }

  // ---------------------------------------------------------------------------
  // Step 4:
  // Submit the command buffer to the device queue. Tell it to signal the render
  // semaphore.
  // ---------------------------------------------------------------------------
  std::vector<VkSemaphore> wait_semaphores = {};
  std::vector<VkSemaphore> queue_signal_semaphores = {
      backbuffer->GetRenderSemaphore()};
  std::vector<VkCommandBuffer> command_buffers = {
      backbuffer->GetRenderCommandBuffer().Handle()};

  if (!device_.QueueSubmit(
          {/* Empty. No wait semaphores. */},  // wait_dest_pipeline_stages
          wait_semaphores,                     // wait_semaphores
          queue_signal_semaphores,             // signal_semaphores
          command_buffers,                     // command_buffers
          backbuffer->GetRenderFence()         // fence
          )) {
    FXL_DLOG(INFO) << "Could not submit to the device queue.";
    return false;
  }

  // ---------------------------------------------------------------------------
  // Step 5:
  // Submit the present operation and wait on the render semaphore.
  // ---------------------------------------------------------------------------
  VkSwapchainKHR swapchain = swapchain_;
  uint32_t present_image_index = static_cast<uint32_t>(current_image_index_);
  const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount =
          static_cast<uint32_t>(queue_signal_semaphores.size()),
      .pWaitSemaphores = queue_signal_semaphores.data(),
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &present_image_index,
      .pResults = nullptr,
  };

  if (VK_CALL_LOG_ERROR(vk.QueuePresentKHR(device_.GetQueueHandle(),
                                           &present_info)) != VK_SUCCESS) {
    FXL_DLOG(INFO) << "Could not submit the present operation.";
    return false;
  }

  return true;
}

}  // namespace vulkan
