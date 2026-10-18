#pragma once
#include "vk_app.h"
#include <vector>

extern bool g_redshift;

void redshiftRamp(double z, float rgb[3], float& bright);
std::vector<UniPoint> redshiftPoints(const std::vector<UniPoint>& pts, unsigned keep);
void redshiftCreateBuffer(AppContext& app, const std::vector<UniPoint>& pts, VkBuffer& buf, VkDeviceMemory& mem);
void redshiftKey(AppContext& app);
void emitRedshiftLines(AppContext& app, OverlayVertex* out, uint32_t& used);
void emitRedshiftText(AppContext& app, OverlayVertex* out, uint32_t& used);
