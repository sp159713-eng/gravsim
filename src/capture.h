#pragma once
#include "vk_app.h"

extern float g_capShotAt;
extern float g_capRecAt;
extern float g_capRecFor;

void captureInit(AppContext& app);
void captureKeys(AppContext& app);
bool captureBeginFrame(AppContext& app);
void captureRecordCopy(AppContext& app, VkCommandBuffer cmd, uint32_t imageIndex);
void captureDraw(AppContext& app, VkCommandBuffer cmd);
void captureShutdown(AppContext& app);
